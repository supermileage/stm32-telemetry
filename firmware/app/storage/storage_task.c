/**
 * @file
 * @brief Storage startup, logger command processing, and shutdown.
 */
#include "storage_task.h"
#include "storage.h"
#include "logging/logger_worker.h"
#include "logging/log_file.h"
#include "FreeRTOS.h"
#include "task.h"

volatile storage_bringup_state_t storage_bringup_state = STORAGE_NOT_STARTED;
volatile FRESULT storage_mount_result = FR_NOT_READY;
volatile FRESULT storage_verify_result = FR_NOT_READY;
volatile FRESULT storage_unmount_result = FR_NOT_READY;
char storage_test_path[STORAGE_TEST_PATH_SIZE];

static StaticTask_t storage_task_control;
static StackType_t storage_task_stack[1024];
static TaskHandle_t storage_task_handle;
static log_file_t log_file;
static bool hello_test_only;

static FRESULT prepare_card(void)
{
    storage_bringup_state = STORAGE_MOUNTING;
    storage_mount_result = storage_mount();
    if (storage_mount_result != FR_OK) {
        storage_bringup_state = STORAGE_MOUNT_FAILED;
        return storage_mount_result;
    }

    storage_bringup_state = STORAGE_READY;
    return FR_OK;
}

static void idle_after_shutdown(void)
{
    /* heap_1 does not support deleting tasks. */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}

static void storage_task(void *argument)
{
    (void)argument;
    FRESULT result = prepare_card();
    if (hello_test_only) {
        if (result == FR_OK) {
            storage_bringup_state = STORAGE_VERIFYING;
            storage_verify_result = storage_self_test(storage_test_path);
            if (storage_verify_result != FR_OK) {
                storage_bringup_state = STORAGE_VERIFY_FAILED;
            }
            result = storage_verify_result;
        }
        storage_unmount_result = storage_unmount();
        if (result == FR_OK) {
            storage_bringup_state = storage_unmount_result == FR_OK ?
                STORAGE_TEST_PASSED : STORAGE_TEST_FAILED;
        }
        idle_after_shutdown();
        return;
    }

    if (result == FR_OK) {
        result = log_file_open(&log_file);
    }
    if (result == FR_OK) {
        logger_worker_ready(log_file.path);
        TickType_t last_sync = xTaskGetTickCount();
        TickType_t interval = pdMS_TO_TICKS(LOGGER_SYNC_INTERVAL_MS);
        if (interval == 0U) {
            interval = 1U;
        }
        bool stopped = false;
        while (result == FR_OK && !stopped) {
            /* Unsigned subtraction keeps the interval valid across a tick wrap. */
            TickType_t elapsed = xTaskGetTickCount() - last_sync;
            TickType_t wait = elapsed < interval ? interval - elapsed : 0U;
            logger_command_t command;
            if (logger_receive(&command, wait)) {
                switch (command.kind) {
                case LOGGER_RECORD:
                    result = log_file_append(&log_file, &command.record);
                    if (result == FR_OK) {
                        logger_worker_processed(log_file.path);
                    }
                    break;
                case LOGGER_FLUSH:
                    result = log_file_sync(&log_file);
                    if (result == FR_OK) {
                        logger_worker_synced();
                    }
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
                result = log_file_sync(&log_file);
                if (result == FR_OK) {
                    logger_worker_synced();
                }
                last_sync = xTaskGetTickCount();
            }
        }
        FRESULT closed = log_file_close(&log_file);
        /* Finish cleanup without hiding the error that stopped logging. */
        if (result == FR_OK) {
            result = closed;
        }
        if (result == FR_OK) {
            logger_worker_synced();
        }
        storage_unmount_result = storage_unmount();
        if (result == FR_OK) {
            result = storage_unmount_result;
        }
        logger_worker_finish(result, stopped);
    } else {
        /* A header sync failure can leave a file handle open. */
        (void)log_file_close(&log_file);
        storage_unmount_result = storage_unmount();
        logger_worker_finish(result, false);
    }
    idle_after_shutdown();
}

static void start_storage(bool test_only)
{
    if (storage_task_handle != NULL) {
        return;
    }
    hello_test_only = test_only;
    if (!test_only) {
        logger_initialize();
    }
    storage_task_handle = xTaskCreateStatic(storage_task, "sd_storage", 1024U,
        NULL, tskIDLE_PRIORITY + 1U, storage_task_stack, &storage_task_control);
    if (storage_task_handle == NULL) {
        storage_mount_result = FR_INT_ERR;
        storage_bringup_state = STORAGE_MOUNT_FAILED;
        if (!test_only) {
            logger_worker_finish(FR_INT_ERR, false);
        }
    }
}

void storage_start_task(void)
{
    start_storage(false);
}

void storage_start_hello_test(void)
{
    start_storage(true);
}
