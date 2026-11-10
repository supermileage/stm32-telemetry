// Shared SPI bus access for FreeRTOS tasks.
// Transactions require a running scheduler and enabled interrupts.
#pragma once

#include "stm32u5xx_hal.h"
#include "FreeRTOS.h"
#include "semphr.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct spi_device spi_device_t;

// One SPI peripheral shared by its devices.
// Zero-initialize one static object per peripheral. Do not copy after init.
typedef struct {
    SPI_HandleTypeDef *handle;

    StaticSemaphore_t mutex_storage;
    SemaphoreHandle_t mutex;

    // Device holding the current transaction.
    const spi_device_t *active_device;
} spi_bus_t;

// A device's bus and chip-select pin.
struct spi_device {
    spi_bus_t *bus;
    GPIO_TypeDef *cs_port;

    // GPIO_PIN_x mask.
    uint16_t cs_pin;
};

// Initialize a bus after CubeMX setup.
// Call before sharing the bus. CS outputs must start high.
// Returns true if ready.
bool spi_bus_init(spi_bus_t *bus, SPI_HandleTypeDef *handle);

// Lock the bus and pull CS low, waiting up to wait_ticks.
// Calls cannot nest. Pair each successful call with spi_end().
// Returns true if acquired.
bool spi_begin(const spi_device_t *device, TickType_t wait_ticks);

// Exchange size bytes without changing CS.
// The caller must own the transaction and call spi_end(), even on failure.
// Returns true if the transfer succeeds.
bool spi_transfer(const spi_device_t *device, const uint8_t *tx, uint8_t *rx,
                  uint16_t size, uint32_t timeout_ms);

// Raise CS and unlock the bus.
// Only the task and device that acquired the bus can release it.
void spi_end(const spi_device_t *device);

// Lock the bus with CS high for SD clocks.
// Uses the same wait and cleanup rules as spi_begin().
// Returns true if acquired.
bool spi_acquire(const spi_device_t *device, TickType_t wait_ticks);

// Change CS while the caller owns the transaction.
// Pass true for CS low, or false for CS high.
// Returns true if updated.
bool spi_select(const spi_device_t *device, bool selected);

#ifdef __cplusplus
}
#endif
