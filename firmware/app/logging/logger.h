/**
 * @file
 * @brief Queue-based logging API for sensor tasks and interrupts.
 */
#ifndef LOGGER_H
#define LOGGER_H

#include "record.h"
#include "FreeRTOS.h"

#include <stdbool.h>

/** Queue capacity shared by records, flush requests, and stop requests. */
#define LOGGER_QUEUE_DEPTH 64U
/** Target time between syncs; scheduling and card latency can delay them. */
#define LOGGER_SYNC_INTERVAL_MS 1000U

/** Logger lifecycle, reported by logger_get_status(). */
typedef enum {
    LOGGER_NOT_STARTED, /**< The command queue has not been created. */
    LOGGER_STARTING,    /**< Accepting records while storage prepares the card. */
    LOGGER_READY,       /**< The log file is open and the worker is consuming commands. */
    LOGGER_STOPPING,    /**< A stop is queued; new records are rejected. */
    LOGGER_STOPPED,     /**< The file was closed and the card unmounted successfully. */
    LOGGER_FAILED       /**< Startup, logging, or shutdown failed; reboot to restart. */
} logger_state_t;

/**
 * Logger state and cumulative record counts since boot.
 * Counters are uint32_t and wrap. Accepted records lost on failure are not added
 * to dropped_records; compare accepted_records with synced_records for progress.
 */
typedef struct {
    logger_state_t state;       /**< Current lifecycle state. */
    uint32_t accepted_records;  /**< Total records successfully queued, including processed ones. */
    uint32_t processed_records; /**< Handed to the file writer; may still be buffered. */
    uint32_t synced_records;    /**< Records covered by the last successful sync or close. */
    uint32_t dropped_records;   /**< Valid records rejected by a full or unavailable logger. */
    uint32_t invalid_records;   /**< Invalid record pointers, lengths, or ISR wake pointers. */
    uint32_t queued_commands;   /**< Pending records plus flush and stop requests. */
    int filesystem_error;       /**< FatFs result from startup, I/O, or shutdown; zero until reported. */
    char path[17];              /**< Last published log path, NUL-terminated; empty before open. */
} logger_status_t;

/**
 * Copy a record to the queue without waiting for space. Call from task context
 * after starting storage. The caller can reuse the record as soon as this returns.
 * Queued records can be lost on failure; synced_records tracks successful syncs.
 * @param record Sample to copy, with length at most LOG_PAYLOAD_MAX.
 * @return true if queued; false for invalid input, a full queue, or an unavailable logger.
 */
bool logger_submit(const log_record_t *record);

/**
 * Submit from an interrupt at a FreeRTOS syscall-safe priority.
 * Initialize higher_priority_task_woken to pdFALSE, then pass it to
 * portYIELD_FROM_ISR after the call. Queue and failure rules match logger_submit().
 * @param record Sample to copy, with length at most LOG_PAYLOAD_MAX.
 * @param[in,out] higher_priority_task_woken Non-NULL FreeRTOS wake flag.
 * @return true if queued; false for invalid input, a full queue, or an unavailable logger.
 */
bool logger_submit_from_isr(const log_record_t *record, BaseType_t *higher_priority_task_woken);

/**
 * Queue a sync after earlier records. Call from task context.
 * A successful request is not a completed sync: use synced_records to track it.
 * @return true if queued; false if the queue is full or the logger is unavailable.
 */
bool logger_request_flush(void);

/**
 * Queue a stop and reject new records once accepted. Retry if the queue is full.
 * The worker drains earlier commands, closes the file, and unmounts the card.
 * Wait for LOGGER_STOPPED before removing the card; LOGGER_FAILED reports an
 * I/O or shutdown error. Restarting the logger requires a reboot.
 * Call from task context.
 * @return true if queued; false if the queue is full or the logger is unavailable.
 */
bool logger_request_stop(void);

/**
 * Copy a consistent status snapshot. Call from task context.
 * @param[out] out Destination for the snapshot; NULL is ignored.
 */
void logger_get_status(logger_status_t *out);

#endif
