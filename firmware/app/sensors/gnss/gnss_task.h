#ifndef GNSS_TASK_H
#define GNSS_TASK_H

#include "stm32u5xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

/** Counters since boot. */
typedef struct {
    uint32_t parsed_records;
    uint32_t rejected_records;
    uint32_t invalid_sentences;
    uint32_t overlong_sentences;
    uint32_t rtc_initializations;
    uint32_t rtc_init_failures;
    uint32_t invalid_utc_sentences;
    uint32_t rx_dropped_bytes;
    uint32_t rx_errors;
    bool rx_active;
} gnss_status_t;

/**
 * Start before scheduling with a configured UART. This task is its only reader.
 * Returns false for invalid arguments or initialization failure.
 */
bool gnss_start_task(UART_HandleTypeDef *uart);

/** Copy counters and UART health. Null output is ignored. */
void gnss_get_status(gnss_status_t *out);

#endif
