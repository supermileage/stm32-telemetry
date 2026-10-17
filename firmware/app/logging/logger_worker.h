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
    LOGGER_RECORD, /**< Append the command's record. */
    LOGGER_FLUSH,  /**< Sync all records processed so far. */
    LOGGER_STOP    /**< Close the log file and unmount the card. */
} logger_command_kind_t;

/** A queue entry; only LOGGER_RECORD uses the record field. */
typedef struct {
    logger_command_kind_t kind; /**< Operation requested by the producer. */
    log_record_t record;        /**< Copy of the sample for a LOGGER_RECORD command. */
} logger_command_t;

/**
 * Create the static queue before the scheduler and producer interrupts start.
 * Sets LOGGER_STARTING on success or LOGGER_FAILED if queue creation fails.
 * Calling again after the queue exists does nothing; this is not a restart API.
 */
void logger_initialize(void);

/**
 * Take the next command, waiting up to the given number of RTOS ticks.
 * @param[out] command Non-NULL destination for the command.
 * @param wait Maximum wait in ticks; zero polls without blocking.
 * @return true if a command was received; false on timeout or before initialization.
 */
bool logger_receive(logger_command_t *command, TickType_t wait);

/**
 * Publish the log path after opening the file and syncing its header.
 * Changes LOGGER_STARTING to LOGGER_READY, preserving an already queued stop.
 * @param path Non-NULL, NUL-terminated filename of at most 16 characters; copied.
 */
void logger_worker_ready(const char *path);

/**
 * Count one successful append and update the path after any file rotation.
 * This records writer progress; it does not mean the sample has been synced.
 * @param path Non-NULL, NUL-terminated filename of at most 16 characters; copied.
 */
void logger_worker_processed(const char *path);

/**
 * Mark all processed records as synced after a successful file sync or close.
 * This only updates the counters; the caller performs the file operation first.
 */
void logger_worker_synced(void);

/**
 * Publish the final state after file cleanup and unmount, then clear the queue.
 * Startup also uses this to report task creation failure before the scheduler starts.
 * Discarded commands do not increase dropped_records. Restart requires a reboot.
 * @param result First startup, I/O, or shutdown error, or FR_OK on a clean stop.
 * @param stopped true if the worker received a stop command.
 */
void logger_worker_finish(FRESULT result, bool stopped);

#endif
