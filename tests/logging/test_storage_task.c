#include "storage/storage_task.h"
#include "storage/storage.h"
#include "logger.h"
#include "log_file.h"
#include "rtos_mock.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

static jmp_buf completed;
static const char *scenario;
static unsigned int mounts, verifies, opens, appends, syncs, closes, unmounts;
static uint32_t first_synced_records;
static bool idle_waited;
static log_file_t *active_file;

static bool is(const char *name)
{
    return strcmp(scenario, name) == 0;
}

static bool fails_at(const char *name)
{
    return is(name) || (strncmp(scenario, "hello_", 6U) == 0 && strcmp(scenario + 6, name) == 0);
}

FRESULT storage_mount(void)
{
    ++mounts;
    return fails_at("mount_failure") ? FR_NOT_READY : FR_OK;
}

FRESULT storage_self_test(char path[17])
{
    ++verifies;
    strcpy(path, "0:/HELLO000.TXT");
    return fails_at("verify_failure") ? FR_DISK_ERR : FR_OK;
}

FRESULT storage_unmount(void)
{
    assert(active_file == NULL || !active_file->open);
    ++unmounts;
    return fails_at("unmount_failure") ? FR_INT_ERR : FR_OK;
}

FRESULT log_file_open(log_file_t *file)
{
    ++opens;
    active_file = file;
    file->open = true; /* A failed header sync can leave an open handle. */
    strcpy(file->path, "0:/LOG00000.BIN");
    return is("open_failure") ? FR_DISK_ERR : FR_OK;
}

FRESULT log_file_append(log_file_t *file, const log_record_t *record)
{
    assert(file->open && record->length == 1U && record->payload[0] == appends);
    ++appends;
    mock_tick += 250U;
    return is("append_failure") ? FR_DISK_ERR : FR_OK;
}

FRESULT log_file_sync(log_file_t *file)
{
    assert(file->open);
    ++syncs;
    if (syncs == 1U) {
        first_synced_records = appends;
    }
    return is("sync_failure") ? FR_DISK_ERR : FR_OK;
}

FRESULT log_file_close(log_file_t *file)
{
    assert(file == active_file || active_file == NULL);
    ++closes;
    file->open = false;
    return is("close_failure") ? FR_DISK_ERR : FR_OK;
}

static void terminal_delay(void)
{
    longjmp(completed, 1);
}

static void on_receive(TickType_t wait)
{
    if (is("idle") && wait != 0U && !idle_waited) {
        idle_waited = true;
        mock_tick += wait; /* No records: the receive timeout must still sync. */
    } else if (is("idle") && syncs != 0U) {
        assert(logger_request_stop());
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    scenario = argv[1];
    bool hello_test = is("hello") || strncmp(scenario, "hello_", 6U) == 0;
    mock_task_creation_fails = fails_at("task_failure");
    mock_delay_hook = terminal_delay;
    mock_receive_hook = on_receive;
    mock_tick = UINT32_MAX - 500U; /* Every run crosses tick wraparound. */
    if (hello_test) {
        storage_start_hello_test();
    } else {
        storage_start_task();
    }
    logger_status_t status;
    if (fails_at("task_failure")) {
        logger_get_status(&status);
        assert(status.state == (hello_test ? LOGGER_NOT_STARTED : LOGGER_FAILED));
        assert(status.filesystem_error == (hello_test ? FR_OK : FR_INT_ERR));
        assert(storage_bringup_state == STORAGE_MOUNT_FAILED && mounts == 0U);
        return 0;
    }
    if (hello_test) {
        storage_start_hello_test();
        storage_start_task(); /* The first call chooses the mode. */
        assert(mock_task_creations == 1U && mock_task_entry != NULL);
        assert(!logger_submit(&(log_record_t){0}));
        if (setjmp(completed) == 0) {
            mock_task_entry(NULL);
        }
        logger_get_status(&status);
        assert(status.state == LOGGER_NOT_STARTED && status.queued_commands == 0U);
        assert(mounts == 1U && unmounts == 1U);
        assert(opens == 0U && appends == 0U && syncs == 0U && closes == 0U);
        storage_bringup_state_t expected = fails_at("mount_failure") ? STORAGE_MOUNT_FAILED :
            fails_at("verify_failure") ? STORAGE_VERIFY_FAILED :
            fails_at("unmount_failure") ? STORAGE_TEST_FAILED : STORAGE_TEST_PASSED;
        assert(storage_bringup_state == expected);
        assert(storage_unmount_result == (fails_at("unmount_failure") ? FR_INT_ERR : FR_OK));
        printf("Hello World task test passed: %s.\n", scenario);
        return 0;
    }
    storage_start_task();
    assert(mock_task_creations == 1U && mock_task_entry != NULL);
    if (!is("idle")) {
        for (uint8_t i = 0U; i < 8U; ++i) {
            log_record_t record = { .timestamp_ms = i, .source = 1U,
                .length = 1U, .payload = {i} };
            assert(logger_submit(&record));
            if (is("flush") && i == 1U) {
                assert(logger_request_flush());
            }
        }
        assert(logger_request_stop());
        assert(!logger_submit(&(log_record_t){0}));
    }
    if (setjmp(completed) == 0) {
        mock_task_entry(NULL);
    }
    logger_get_status(&status);
    assert(mounts == 1U && unmounts == 1U && closes == 1U);
    assert(active_file == NULL || !active_file->open);
    assert(status.queued_commands == 0U);
    bool success = is("normal") || is("flush") || is("idle");
    assert(status.state == (success ? LOGGER_STOPPED : LOGGER_FAILED));
    FRESULT expected = is("mount_failure") ? FR_NOT_READY :
        is("unmount_failure") ? FR_INT_ERR : success ? FR_OK : FR_DISK_ERR;
    assert(status.filesystem_error == (int)expected);
    if (success) {
        assert(status.accepted_records == status.processed_records);
        assert(status.synced_records == status.processed_records);
        if (is("normal")) {
            assert(first_synced_records == 4U && syncs == 2U);
        }
        if (is("flush")) {
            assert(first_synced_records == 2U && syncs == 2U);
        }
        if (is("idle")) {
            assert(idle_waited && appends == 0U && syncs == 1U);
        }
    } else {
        assert(!logger_request_flush() && !logger_request_stop());
        if (is("mount_failure")) {
            assert(verifies == 0U && opens == 0U);
        }
        if (is("verify_failure")) {
            assert(opens == 0U);
        }
        if (is("append_failure")) {
            assert(status.processed_records == 0U && appends == 1U);
        }
        if (is("sync_failure")) {
            assert(status.synced_records == 0U);
        }
        /* A later close error must preserve earlier successful sync progress. */
        if (is("close_failure")) {
            assert(status.synced_records == 8U && syncs == 2U);
        }
    }
    printf("Storage task test passed: %s.\n", scenario);
    return 0;
}
