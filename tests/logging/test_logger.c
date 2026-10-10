#include "logger_worker.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    (void)argv;
    logger_status_t status;
    logger_get_status(&status);
    assert(status.state == LOGGER_NOT_STARTED);
    log_record_t record = { .timestamp_ms = 123, .source = 7, .length = 3, .payload = {1, 2, 3} };
    assert(!logger_submit(&record));
    logger_initialize();
    logger_initialize();
    assert(!logger_submit(NULL));
    record.length = LOG_PAYLOAD_MAX + 1U;
    assert(!logger_submit(&record));
    record.length = 3;
    assert(logger_submit(&record));
    record.payload[0] = 99;
    logger_command_t command;
    assert(logger_receive(&command, 0U) && command.record.payload[0] == 1);
    logger_worker_processed("0:/LOG00000.BIN");
    BaseType_t woken = pdFALSE;
    assert(logger_submit_from_isr(&record, &woken) && woken == pdTRUE);
    assert(!logger_submit_from_isr(&record, NULL));
    for (unsigned int i = 1; i < LOGGER_QUEUE_DEPTH; ++i) assert(logger_submit(&record));
    assert(!logger_submit(&record));
    assert(!logger_submit_from_isr(&record, &woken));
    assert(!logger_request_flush());
    assert(!logger_request_stop());
    logger_get_status(&status);
    assert(status.state == LOGGER_STARTING && status.queued_commands == LOGGER_QUEUE_DEPTH);
    while (logger_receive(&command, 0U)) logger_worker_processed("0:/LOG00000.BIN");
    assert(logger_request_flush());
    assert(logger_submit(&record));
    assert(logger_request_stop());
    assert(!logger_submit(&record) && !logger_submit_from_isr(&record, &woken));
    logger_worker_ready("0:/LOG00000.BIN");
    logger_get_status(&status);
    assert(status.state == LOGGER_STOPPING);
    assert(logger_receive(&command, 0U) && command.kind == LOGGER_FLUSH);
    logger_worker_synced();
    assert(logger_receive(&command, 0U) && command.kind == LOGGER_RECORD);
    logger_worker_processed("0:/LOG00000.BIN");
    assert(logger_receive(&command, 0U) && command.kind == LOGGER_STOP);
    logger_worker_synced();
    logger_worker_finish(argc > 1 ? FR_DISK_ERR : FR_OK, true);
    logger_get_status(&status);
    assert(status.state == (argc > 1 ? LOGGER_FAILED : LOGGER_STOPPED));
    assert(status.filesystem_error == (argc > 1 ? FR_DISK_ERR : FR_OK));
    assert(status.accepted_records == LOGGER_QUEUE_DEPTH + 2U);
    assert(status.processed_records == status.accepted_records);
    assert(status.synced_records == status.processed_records);
    assert(status.invalid_records == 3U && status.dropped_records == 5U);
    assert(status.queued_commands == 0U && strcmp(status.path, "0:/LOG00000.BIN") == 0);
    assert(!logger_request_flush() && !logger_request_stop() && !logger_submit(&record));
    puts("Logger tests passed: copied records, ISR, overflow, ordering, shutdown, status.");
    return 0;
}
