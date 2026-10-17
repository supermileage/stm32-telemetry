/**
 * @file
 * @brief Static logger queue and shared status for producers and the storage task.
 */
#include "logger_worker.h"
#include "queue.h"
#include "task.h"

#include <string.h>

static StaticQueue_t queue_control;
static uint8_t queue_storage[LOGGER_QUEUE_DEPTH * sizeof(logger_command_t)];
static QueueHandle_t commands;
static logger_status_t status;

static bool can_accept_records(void)
{
    return commands != NULL && (status.state == LOGGER_STARTING || status.state == LOGGER_READY);
}

void logger_initialize(void)
{
    if (commands != NULL) {
        return;
    }
    commands = xQueueCreateStatic(LOGGER_QUEUE_DEPTH, sizeof(logger_command_t),
        queue_storage, &queue_control);
    status.state = commands != NULL ? LOGGER_STARTING : LOGGER_FAILED;
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

bool logger_receive(logger_command_t *command, TickType_t wait)
{
    return commands != NULL && xQueueReceive(commands, command, wait) == pdPASS;
}

void logger_worker_ready(const char *path)
{
    taskENTER_CRITICAL();
    if (status.state == LOGGER_STARTING) {
        status.state = LOGGER_READY;
    }
    (void)strncpy(status.path, path, sizeof(status.path) - 1U);
    taskEXIT_CRITICAL();
}

void logger_worker_processed(const char *path)
{
    taskENTER_CRITICAL();
    ++status.processed_records;
    (void)strncpy(status.path, path, sizeof(status.path) - 1U);
    taskEXIT_CRITICAL();
}

void logger_worker_synced(void)
{
    taskENTER_CRITICAL();
    status.synced_records = status.processed_records;
    taskEXIT_CRITICAL();
}

void logger_worker_finish(FRESULT result, bool stopped)
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
