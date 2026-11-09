/**
 * @file uart_port.h
 * @brief Buffered interrupt RX and blocking TX for one UART.
 * After init, APIs require task context and a running scheduler.
 * Keep interrupts enabled.
 */
#pragma once

#include "stm32u5xx_hal.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "stream_buffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { UART_PORT_RX_BUFFER_SIZE = 256U };

typedef struct {
    bool active;             /**< RX is armed. */
    uint32_t dropped_bytes;   /**< Bytes lost when the buffer was full. */
    uint32_t errors;          /**< Receive errors and failed RX starts. */
} uart_rx_status_t;

/**
 * @brief State for one UART port.
 * Zero-initialize one static object per UART. Do not copy after init.
 */
typedef struct {
    UART_HandleTypeDef *handle;

    StaticStreamBuffer_t rx_control;
    /* FreeRTOS reserves one slot to distinguish a full ring from an empty one. */
    uint8_t rx_storage[UART_PORT_RX_BUFFER_SIZE + 1U];
    StreamBufferHandle_t rx_stream;

    StaticSemaphore_t tx_mutex_storage;
    SemaphoreHandle_t tx_mutex;

    uint8_t rx_byte;
    /* ISR writes. Task access uses short critical sections. */
    volatile bool rx_active;
    volatile uint32_t rx_dropped_bytes;
    volatile uint32_t rx_errors;
} uart_port_t;

/**
 * @brief Initialize a CubeMX UART before the scheduler starts.
 * Requires 8-bit words, no parity and FIFO off. Call once per port.
 * @return true if ready.
 */
bool uart_port_init(uart_port_t *port, UART_HandleTypeDef *handle);

/**
 * @brief Start interrupt RX from the reader task.
 * The UART IRQ must be enabled at a FreeRTOS-safe priority.
 * @return true if RX is active.
 */
bool uart_port_start_rx(uart_port_t *port);

/**
 * @brief Read up to capacity buffered bytes from the single reader task.
 * Wait up to timeout_ticks. Returns bytes read, or zero on timeout or failure.
 */
size_t uart_port_read(uart_port_t *port, uint8_t *destination, size_t capacity,
                     TickType_t timeout_ticks);

/**
 * @brief Send bytes with mutex protection.
 * wait_ticks limits mutex waiting; portMAX_DELAY waits indefinitely.
 * timeout_ms separately limits transmission; HAL_MAX_DELAY waits indefinitely.
 * Zero polls the respective stage. Failure may follow a partial transmission.
 * @return true if all bytes were sent.
 */
bool uart_port_write(uart_port_t *port, const uint8_t *data, uint16_t length,
                     TickType_t wait_ticks, uint32_t timeout_ms);

/**
 * @brief Read a consistent snapshot of RX state and counters.
 */
uart_rx_status_t uart_port_rx_status(const uart_port_t *port);

#ifdef __cplusplus
}
#endif
