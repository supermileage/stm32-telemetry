// Buffered UART reception and blocking transmission.
// After setup, call these functions from a running task with interrupts enabled.
#pragma once

#include "stm32u5xx_hal.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { UART_PORT_RX_BUFFER_SIZE = 256U };

typedef struct {
    // Time this byte arrived, from HAL_GetTick().
    uint32_t tick_ms;
    uint8_t byte;
} uart_rx_byte_t;

typedef struct {
    // Reception is running.
    bool active;

    // Bytes dropped because the receive queue was full.
    uint32_t dropped_bytes;

    // UART errors and failed attempts to start reception.
    uint32_t errors;
} uart_rx_status_t;

// State for one UART port.
// Zero-initialize one static object per UART. Do not copy after init.
typedef struct {
    UART_HandleTypeDef *handle;

    StaticQueue_t rx_control;
    uint8_t rx_storage[UART_PORT_RX_BUFFER_SIZE * sizeof(uart_rx_byte_t)];
    QueueHandle_t rx_queue;

    StaticSemaphore_t tx_mutex_storage;
    SemaphoreHandle_t tx_mutex;

    uint8_t rx_byte;

    // The interrupt updates these values. Tasks read them in short critical sections.
    volatile bool rx_active;
    volatile uint32_t rx_dropped_bytes;
    volatile uint32_t rx_errors;
} uart_port_t;

// Initialize a CubeMX UART before the scheduler starts.
// Requires 8-bit words, no parity and FIFO off. Call once per port.
// Returns true if ready.
bool uart_port_init(uart_port_t *port, UART_HandleTypeDef *handle);

// Start interrupt RX from the reader task.
// The UART IRQ must be enabled at a FreeRTOS-safe priority.
// Returns true if RX is active.
bool uart_port_start_rx(uart_port_t *port);

// Read up to capacity bytes from the port's reader task.
// Wait up to timeout_ticks for the first byte. Returns the number read,
// or zero if no data arrives or the call fails.
size_t uart_port_read(uart_port_t *port, uint8_t *destination, size_t capacity,
                     TickType_t timeout_ticks);

// Read bytes with the times saved when they arrived.
// Capacity counts entries, not bytes. Only the first entry can wait for data.
// Use the same reader task as uart_port_read().
size_t uart_port_read_timed(uart_port_t *port, uart_rx_byte_t *destination,
                           size_t capacity, TickType_t timeout_ticks);

// Send bytes with mutex protection.
// wait_ticks limits the mutex wait. Use portMAX_DELAY to wait indefinitely.
// timeout_ms limits transmission. Use HAL_MAX_DELAY to wait indefinitely.
// Use zero to avoid waiting. A failed send may have transmitted some bytes.
// Returns true if all bytes were sent.
bool uart_port_write(uart_port_t *port, const uint8_t *data, uint16_t length,
                     TickType_t wait_ticks, uint32_t timeout_ms);

// Read a consistent snapshot of RX state and counters.
uart_rx_status_t uart_port_rx_status(const uart_port_t *port);

#ifdef __cplusplus
}
#endif
