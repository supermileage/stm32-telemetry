/**
 * @file
 * @brief Queue consumer and status updates used by the storage task.
 *
 * Producers use logger.h. Initialize before the scheduler and producer interrupts
 * start. The storage task uses the remaining functions; startup also reports task
 * creation failure through logger_worker_finish(). None of these calls are ISR-safe.
 */
#ifndef LOGGER_WORKER_H
#define LOGGER_WORKER_H

#include "logger.h"
#include "ff.h"

/** Commands are processed in queue order, so flush and stop follow earlier records. */
typedef enum {
    /** Append the command's record. */
    LOGGER_RECORD,
    /** Sync all records processed so far. */
    LOGGER_FLUSH,
    /** Close the log file and unmount the card. */
    LOGGER_STOP
} logger_command_kind_t;

/** A queue entry; only LOGGER_RECORD uses the record field. */
typedef struct {
    /** Operation requested by the producer. */
    logger_command_kind_t kind;
    /** Copy of the sample for a LOGGER_RECORD command. */
    log_record_t record;
} logger_command_t;

/**
 * @brief Create the queue before the scheduler and producer interrupts start.
 * Sets LOGGER_STARTING or LOGGER_FAILED. Repeated calls do not restart the logger.
 */
void logger_initialize(void);

/**
 * @brief Take the next queued command.
 * @param[out] command Non-NULL command destination.
 * @param wait Timeout in RTOS ticks; zero polls.
 * @return true if received; false on timeout or before initialization.
 */
bool logger_receive(logger_command_t *command, TickType_t wait);

/**
 * @brief Publish readiness after opening and syncing the file header.
 * Preserves an already queued stop.
 * @param path Log path to copy, NUL-terminated and at most 16 characters.
 */
void logger_worker_ready(const char *path);

/**
 * @brief Count a successful append and publish the current path.
 * Buffered records may not be synced yet.
 * @param path Log path to copy, NUL-terminated and at most 16 characters.
 */
void logger_worker_processed(const char *path);

/**
 * @brief Mark processed records as synced after a successful sync or close.
 */
void logger_worker_synced(void);

/**
 * @brief Publish the final state and discard remaining commands after cleanup.
 * Also reports startup failures. Discarded records do not increase dropped_records.
 * @param result First startup, I/O, or cleanup error; FR_OK for a clean stop.
 * @param stopped true if a stop command was received.
 */
void logger_worker_finish(FRESULT result, bool stopped);

#endif
