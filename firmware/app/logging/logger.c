#include "logger.h"
#include "log_file.h"
#include "storage/storage.h"
#include "queue.h"
#include "task.h"

#include <string.h>

/* One FIFO keeps flush and stop requests ordered after earlier records. */
typedef enum {
    LOGGER_RECORD,
    LOGGER_FLUSH,
    LOGGER_STOP
} logger_command_kind_t;

typedef struct {
    logger_command_kind_t kind;
    log_record_t record;
} logger_command_t;

static StaticQueue_t queue_control;
static uint8_t queue_storage[LOGGER_QUEUE_DEPTH * sizeof(logger_command_t)];
static QueueHandle_t commands;
/* Task and syscall-safe ISR access is protected by FreeRTOS critical sections. */
static logger_status_t status;

static StaticTask_t task_control;
/* Stack depths count StackType_t entries, not bytes (4 KiB on this port). */
enum { LOGGER_STACK_DEPTH = 1024U };
static StackType_t task_stack[LOGGER_STACK_DEPTH];
static log_file_t log_file;

static bool can_accept_records(void)
{
    return commands != NULL && (status.state == LOGGER_STARTING || status.state == LOGGER_READY);
}

static void finish(FRESULT result, bool stopped)
{
    taskENTER_CRITICAL();
    status.filesystem_error = (int)result;
    status.state = result == FR_OK && stopped ? LOGGER_STOPPED : LOGGER_FAILED;
    taskEXIT_CRITICAL();
    /* Producers now reject new records, so the remaining queue can be cleared. */
    if (commands != NULL) {
        (void)xQueueReset(commands);
    }
}

static FRESULT sync_log(void)
{
    FRESULT result = log_file_sync(&log_file);
    if (result == FR_OK) {
        taskENTER_CRITICAL();
        status.synced_records = status.processed_records;
        taskEXIT_CRITICAL();
    }
    return result;
}

static void logger_task(void *argument)
{
    (void)argument;
    FRESULT result = storage_mount();
    if (result == FR_OK) {
        result = log_file_open(&log_file);
    }
    bool stopped = false;
    if (result == FR_OK) {
        taskENTER_CRITICAL();
        /* A stop may already have been queued while the card was mounting. */
        if (status.state == LOGGER_STARTING) {
            status.state = LOGGER_READY;
        }
        (void)strncpy(status.path, log_file.path, sizeof(status.path) - 1U);
        taskEXIT_CRITICAL();

        TickType_t last_sync = xTaskGetTickCount();
        TickType_t interval = pdMS_TO_TICKS(LOGGER_SYNC_INTERVAL_MS);
        if (interval == 0U) {
            interval = 1U;
        }
        while (result == FR_OK && !stopped) {
            /* Unsigned subtraction keeps the interval valid across a tick wrap. */
            TickType_t elapsed = xTaskGetTickCount() - last_sync;
            TickType_t wait = elapsed < interval ? interval - elapsed : 0U;
            logger_command_t command;
            if (xQueueReceive(commands, &command, wait) == pdPASS) {
                switch (command.kind) {
                case LOGGER_RECORD:
                    result = log_file_append(&log_file, &command.record);
                    if (result == FR_OK) {
                        taskENTER_CRITICAL();
                        ++status.processed_records;
                        (void)strncpy(status.path, log_file.path, sizeof(status.path) - 1U);
                        taskEXIT_CRITICAL();
                    }
                    break;
                case LOGGER_FLUSH:
                    result = sync_log();
                    last_sync = xTaskGetTickCount();
                    break;
                case LOGGER_STOP:
                    stopped = true;
                    break;
                }
            }
            /* A busy producer must not postpone the periodic sync. */
            if (result == FR_OK && !stopped &&
                (TickType_t)(xTaskGetTickCount() - last_sync) >= interval) {
                result = sync_log();
                last_sync = xTaskGetTickCount();
            }
        }
    }

    /* Even a failed header sync can leave an open file. Preserve the first error. */
    FRESULT closed = log_file_close(&log_file);
    if (result == FR_OK) {
        result = closed;
    }
    if (result == FR_OK) {
        taskENTER_CRITICAL();
        status.synced_records = status.processed_records;
        taskEXIT_CRITICAL();
    }
    FRESULT unmounted = storage_unmount();
    if (result == FR_OK) {
        result = unmounted;
    }
    finish(result, stopped);
    /* Reboot to restart. If resumed accidentally, suspend again without returning. */
    for (;;) {
        vTaskSuspend(NULL);
    }
}

void logger_start_task(void)
{
    /* Startup only: the scheduler and producer interrupts must not be running. */
    if (status.state != LOGGER_NOT_STARTED) {
        return;
    }
    commands = xQueueCreateStatic(LOGGER_QUEUE_DEPTH, sizeof(logger_command_t),
        queue_storage, &queue_control);
    status.state = LOGGER_STARTING;
    if (commands == NULL || xTaskCreateStatic(logger_task, "sd_logger", LOGGER_STACK_DEPTH,
        NULL, tskIDLE_PRIORITY + 1U, task_stack, &task_control) == NULL) {
        finish(FR_INT_ERR, false);
    }
}

bool logger_submit(const log_record_t *record)
{
    if (record == NULL || record->length > LOG_PAYLOAD_MAX) {
        taskENTER_CRITICAL();
        ++status.invalid_records;
        taskEXIT_CRITICAL();
        return false;
    }
    logger_command_t command = { .kind = LOGGER_RECORD, .record = *record };
    /* Keep the state check, enqueue, and count together so stop cannot slip between them. */
    taskENTER_CRITICAL();
    bool accepted = can_accept_records() && xQueueSend(commands, &command, 0U) == pdPASS;
    if (accepted) {
        ++status.accepted_records;
    } else {
        ++status.dropped_records;
    }
    taskEXIT_CRITICAL();
    return accepted;
}

bool logger_submit_from_isr(const log_record_t *record, BaseType_t *higher_priority_task_woken)
{
    UBaseType_t mask = taskENTER_CRITICAL_FROM_ISR();
    bool accepted = false;
    if (record == NULL || higher_priority_task_woken == NULL || record->length > LOG_PAYLOAD_MAX) {
        ++status.invalid_records;
    } else {
        logger_command_t command = { .kind = LOGGER_RECORD, .record = *record };
        accepted = can_accept_records() &&
            xQueueSendFromISR(commands, &command, higher_priority_task_woken) == pdPASS;
        if (accepted) {
            ++status.accepted_records;
        } else {
            ++status.dropped_records;
        }
    }
    taskEXIT_CRITICAL_FROM_ISR(mask);
    return accepted;
}

static bool request(logger_command_kind_t kind)
{
    logger_command_t command = { .kind = kind };
    taskENTER_CRITICAL();
    bool accepted = can_accept_records() && xQueueSend(commands, &command, 0U) == pdPASS;
    if (accepted && kind == LOGGER_STOP) {
        status.state = LOGGER_STOPPING;
    }
    taskEXIT_CRITICAL();
    return accepted;
}

bool logger_request_flush(void)
{
    return request(LOGGER_FLUSH);
}

bool logger_request_stop(void)
{
    return request(LOGGER_STOP);
}

void logger_get_status(logger_status_t *out)
{
    if (out == NULL) {
        return;
    }
    taskENTER_CRITICAL();
    *out = status;
    out->queued_commands = commands == NULL ? 0U : (uint32_t)uxQueueMessagesWaiting(commands);
    taskEXIT_CRITICAL();
}
