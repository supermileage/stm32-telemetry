#pragma once
#include "stm32u5xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Counters since boot. */
typedef struct {
    uint32_t parsed_packets;
    uint32_t framing_errors;
    uint32_t rejected_records;
    uint32_t stream_resets;
    uint32_t rx_dropped_bytes;
    uint32_t rx_errors;
    bool rx_active;
} fcc_status_t;

/**
 * Start before scheduling with a configured UART and verified gap (see README.md).
 * Returns false for invalid arguments or initialization failure.
 */
bool fcc_start_task(UART_HandleTypeDef *uart, uint32_t gap_ms);

/** Copy counters and UART health from a task. Null output is ignored. */
void fcc_get_status(fcc_status_t *out);

#ifdef __cplusplus
}
#endif
