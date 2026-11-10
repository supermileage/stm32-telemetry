// Read GPS data and send RMC records to the logger.
#ifndef GNSS_TASK_H
#define GNSS_TASK_H

#include "stm32u5xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // Parsed RMC messages, including messages without a fix.
    uint32_t parsed_records;

    // Samples the logger could not accept.
    uint32_t rejected_records;

    // Messages with bad checksums or invalid fields.
    uint32_t invalid_sentences;

    // Sentences too long for the input buffer.
    uint32_t overlong_sentences;

    // GPS messages accepted as UTC updates.
    uint32_t utc_updates;

    // GPS messages with invalid, missing, or old UTC.
    uint32_t rejected_utc_updates;

    // Bytes dropped by the UART receive queue.
    uint32_t rx_dropped_bytes;

    // UART errors and failed attempts to start reception.
    uint32_t rx_errors;
    bool rx_active;
} gnss_status_t;

// Set up the UART and create the GNSS task using static storage.
// Call after CubeMX UART setup and logger_start_task(), before the scheduler.
// UART baud rate must match the receiver; use 8-bit data, no parity, FIFO off,
// and an enabled FreeRTOS-safe UART IRQ. Only this task reads the port.
// Calling again with the same UART does not create another task.
// Returns false for invalid setup or task creation failure.
bool gnss_start_task(UART_HandleTypeDef *uart);

// Copy task counters and UART state. Running task context only; NULL is ignored.
void gnss_get_status(gnss_status_t *out);

#ifdef __cplusplus
}
#endif

#endif
