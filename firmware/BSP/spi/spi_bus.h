/**
 * @file spi_bus.h
 * @brief Blocking SPI transactions shared between FreeRTOS tasks.
 */
#pragma once

#include "stm32u5xx_hal.h"
#include "FreeRTOS.h"
#include "semphr.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Device connected to a shared SPI bus. */
typedef struct spi_device spi_device_t;

/**
 * @brief One SPI peripheral and the mutex shared by its devices.
 *
 * All devices on that bus use the same object.
 * Zero-initialize it, and don't copy it once the mutex has been created.
 */
typedef struct {
    /** HAL handle initialized by CubeMX. */
    SPI_HandleTypeDef *handle;
    /** Storage for the mutex, so no heap allocation is needed. */
    StaticSemaphore_t mutex_storage;
    /** Mutex used to lock the bus for a whole transaction. */
    SemaphoreHandle_t mutex;
    /** Device holding the bus, or NULL when the bus is free. */
    const spi_device_t *active_device;
} spi_bus_t;

/** A device's bus and chip-select pin. */
struct spi_device {
    /** Bus shared by all devices on this SPI peripheral. */
    spi_bus_t *bus;
    /** GPIO port used for chip select. */
    GPIO_TypeDef *cs_port;
    /** GPIO_PIN_x mask for chip select, not a pin number. */
    uint16_t cs_pin;
};

/**
 * @brief Create the mutex for a CubeMX-initialized SPI peripheral.
 *
 * Initialize once, without concurrent callers, before tasks use the bus.
 * Fixed SPI settings are checked with configASSERT().
 * Use master, full-duplex 8-bit, MSB-first SPI with software NSS and no TI mode or CRC.
 * Configure CS outputs high.
 *
 * @param bus Zero-initialized, long-lived bus object.
 * @param handle Initialized HAL SPI handle.
 * @return true if ready; false for invalid input, interrupt context, an unready
 *         peripheral, a different handle on an existing bus, or mutex failure.
 */
bool spi_bus_init(spi_bus_t *bus, SPI_HandleTypeDef *handle);

/**
 * @brief Lock the bus and assert the device's CS low.
 *
 * Task context only, with the scheduler running. Calls cannot be nested.
 * Mode and clock stay at their CubeMX settings; all devices must support them.
 * @param device Device to keep valid and unchanged until spi_end().
 * @param wait_ticks Mutex timeout in FreeRTOS ticks.
 * @return true if acquired; false for invalid input, wrong context, nesting, or timeout.
 */
bool spi_begin(const spi_device_t *device, TickType_t wait_ticks);

/**
 * @brief Perform a blocking, full-duplex 8-bit transfer without changing CS.
 *
 * CS stays low across transfers after spi_begin(). Failure leaves ownership
 * unchanged; the caller must still call spi_end(). Task context only.
 * @param device Device holding the bus in the calling task.
 * @param tx Buffer of size bytes; use dummy bytes such as 0xFF when reading.
 * @param[out] rx Buffer with room for size bytes.
 * @param size Transfer length, from 1 to 65535 bytes.
 * @param timeout_ms HAL timeout in milliseconds.
 * @return true on success; false for invalid input, missing ownership, or HAL failure.
 */
bool spi_transfer(const spi_device_t *device, const uint8_t *tx, uint8_t *rx,
                  uint16_t size, uint32_t timeout_ms);

/**
 * @brief Raise CS and unlock the bus, including after a failed transfer.
 * @param device Device that acquired the bus in this task.
 * Calls outside running task context or without ownership are ignored.
 */
void spi_end(const spi_device_t *device);

/* SD compatibility: keep the mutex while clocking with CS high.
 * Ordinary devices should use spi_begin(), spi_transfer(), and spi_end(). */

/**
 * @brief Lock the bus with CS high for protocols requiring deselected clocks.
 * Same context, lifetime, timeout, and nesting rules as spi_begin().
 * Release with spi_end(), including after a failed transfer.
 * @return true if acquired; false for invalid input, wrong context, nesting, or timeout.
 */
bool spi_acquire(const spi_device_t *device, TickType_t wait_ticks);

/**
 * @brief Change CS without releasing the mutex, for SD protocol callbacks.
 * @param device Device holding the bus in the calling task.
 * @param selected true for CS low; false for CS high.
 * @return true if updated; false if the caller does not own the bus.
 */
bool spi_select(const spi_device_t *device, bool selected);

#ifdef __cplusplus
}
#endif
