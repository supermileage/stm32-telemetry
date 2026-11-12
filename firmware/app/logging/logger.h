/** Queue sensor measurements for the task that writes them to the SD card. */
#ifndef LOGGER_H
#define LOGGER_H

#include "record.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Records and stop requests share this many queue slots. */
#define LOGGER_QUEUE_DEPTH 64U

/** Sync about once per second. Scheduling and card latency can delay a sync. */
#define LOGGER_SYNC_INTERVAL_MS 1000U

#define LOGGER_PATH_SIZE 32U

/** Recording state returned by logger_get_status(). */
typedef enum {
    /** The queue and worker have not been created. */
    LOGGER_NOT_STARTED,

    /** Records can be queued while the worker mounts the card and opens files. */
    LOGGER_STARTING,

    /** Files are open and the worker is processing queued records. */
    LOGGER_READY,

    /** A stop is queued. New records are rejected. */
    LOGGER_STOPPING,

    /** All files were closed and the card was unmounted successfully. */
    LOGGER_STOPPED,

    /** Startup, writing, or shutdown failed. Reboot to try again. */
    LOGGER_FAILED
} logger_state_t;

/**
 * Recording progress since boot.
 */
typedef struct {
    logger_state_t state;

    /** All records accepted into the queue, including ones already processed. */
    uint32_t accepted_records;

    /** Records handed to the file writer. Some may still be buffered in RAM. */
    uint32_t processed_records;

    /** Records covered by the last successful sync or close. */
    uint32_t synced_records;

    /** Valid records rejected because the queue was full or logging unavailable. */
    uint32_t dropped_records;

    /** Submissions with a null record or an unsupported source. */
    uint32_t invalid_records;

    /** Records and stop requests still waiting in the queue. */
    uint32_t queued_commands;

    /** First startup, write, or shutdown error. Zero until an error is reported. */
    int filesystem_error;

    /** Recording folder. Empty until a folder has been created. */
    char path[LOGGER_PATH_SIZE];
} logger_status_t;

/**
 * Create the queue and the task that owns all SD card access.
 * Call once before the scheduler starts. The worker mounts the card and opens
 * files when it begins running. Check logger_get_status() for the result.
 * Later calls do nothing. Reboot to restart after a stop or failure.
 */
void logger_start_task(void);

/**
 * Copy a record into the message queue
 */
bool logger_submit(const log_record_t *record);

/**
 * Queue a stop after the records already waiting.
 */
bool logger_request_stop(void);

/** Copy recording progress from a task. */
void logger_get_status(logger_status_t *out);

#ifdef __cplusplus
}
#endif

#endif
