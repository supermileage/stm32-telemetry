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
    /** The command queue has not been created. */
    LOGGER_NOT_STARTED,
    /** Accepting records while storage prepares the card. */
    LOGGER_STARTING,
    /** The log file is open and the worker is consuming commands. */
    LOGGER_READY,
    /** A stop is queued; new records are rejected. */
    LOGGER_STOPPING,
    /** The file was closed and the card unmounted successfully. */
    LOGGER_STOPPED,
    /** Startup, logging, or shutdown failed; reboot to restart. */
    LOGGER_FAILED
} logger_state_t;

/**
 * Logger state and cumulative record counts since boot.
 * Counters are uint32_t and wrap. Accepted records lost on failure are not added
 * to dropped_records; compare accepted_records with synced_records for progress.
 */
typedef struct {
    /** Current lifecycle state. */
    logger_state_t state;
    /** Total records successfully queued, including processed ones. */
    uint32_t accepted_records;
    /** Handed to the file writer; may still be buffered. */
    uint32_t processed_records;
    /** Records covered by the last successful sync or close. */
    uint32_t synced_records;
    /** Valid records rejected by a full or unavailable logger. */
    uint32_t dropped_records;
    /** Invalid record pointers, lengths, or ISR wake pointers. */
    uint32_t invalid_records;
    /** Pending records plus flush and stop requests. */
    uint32_t queued_commands;
    /** FatFs result from startup, I/O, or shutdown; zero until reported. */
    int filesystem_error;
    /** Last published log path, NUL-terminated; empty before open. */
    char path[17];
} logger_status_t;

/**
 * @brief Copy a record to the queue without waiting for space.
 * Call from a task after storage starts. Accepted records can still be lost on I/O failure.
 * @param record Sample with length at most LOG_PAYLOAD_MAX; reusable after this call.
 * @return true if queued; false for invalid input, a full queue, or an unavailable logger.
 */
bool logger_submit(const log_record_t *record);

/**
 * @brief Queue a record from a FreeRTOS syscall-safe interrupt.
 * @param record Sample to copy, with length at most LOG_PAYLOAD_MAX.
 * @param[in,out] higher_priority_task_woken Initialize to pdFALSE; pass to portYIELD_FROM_ISR afterward.
 * @return true if queued; false for invalid input, a full queue, or an unavailable logger.
 */
bool logger_submit_from_isr(const log_record_t *record, BaseType_t *higher_priority_task_woken);

/**
 * @brief Queue a sync after earlier records. Task context only.
 * Use synced_records to check completion.
 * @return true if queued; false if the queue is full or the logger is unavailable.
 */
bool logger_request_flush(void);

/**
 * @brief Queue a stop and reject further records once accepted.
 * Task context only. Retry if full, then wait for LOGGER_STOPPED before removing
 * the card. LOGGER_FAILED reports an error; restarting requires a reboot.
 * @return true if queued; false if the queue is full or the logger is unavailable.
 */
bool logger_request_stop(void);

/**
 * @brief Copy a consistent logger status snapshot. Task context only.
 * @param[out] out Snapshot destination; NULL is ignored.
 */
void logger_get_status(logger_status_t *out);

#endif
