/** Receive GPS data, initialize the RTC, and send measurements to the logger. */
#ifndef GNSS_TASK_H
#define GNSS_TASK_H

#include "stm32u5xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

/** GPS reception and logging counters since boot. The 32-bit counters can wrap. */
typedef struct {
    /** Parsed RMC messages, including messages without a fix. */
    uint32_t parsed_records;

    /** Measurements the logger could not accept. */
    uint32_t rejected_records;

    /** Sentences rejected for bad checksums, damaged characters, or invalid fields. */
    uint32_t invalid_sentences;

    /** Sentences that did not fit in the parser's line buffer. */
    uint32_t overlong_sentences;

    /** Successful RTC initializations from GPS. At most one per boot. */
    uint32_t rtc_initializations;

    /** Failed RTC initialization attempts. A later valid message can retry. */
    uint32_t rtc_init_failures;

    /** RMC messages without a valid fix or a calendar supported by the RTC. */
    uint32_t invalid_utc_sentences;

    /** Bytes dropped because the UART receive queue was full. */
    uint32_t rx_dropped_bytes;

    /** UART errors and failed attempts to start reception. */
    uint32_t rx_errors;

    /** Whether UART reception is currently running. */
    bool rx_active;
} gnss_status_t;

/**
 * Set up the UART reader and create the GPS task using static storage.
 * Call after CubeMX UART setup and logger_start_task(), before scheduling starts.
 * Match the receiver's baud rate and use 8-bit data, no parity, and FIFO off.
 * Enable the UART interrupt at a priority allowed to call FreeRTOS.
 * This task must be the port's only reader. Repeating setup with the same UART
 * reuses the existing task. Returns false if setup or task creation fails.
 */
bool gnss_start_task(UART_HandleTypeDef *uart);

/** Copy counters and UART state from a running task. A null pointer is ignored. */
void gnss_get_status(gnss_status_t *out);

#endif
