/**
 * @file
 * @brief STM32 HAL SPI and chip-select callbacks for the SD driver.
 */
#ifndef SD_SPI_PORT_H
#define SD_SPI_PORT_H

#include "stm32u5xx_hal.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Configure a dedicated SPI bus and active-low CS output for the SD driver.
 * Register its hardware callbacks, removing any previous callbacks on failure.
 * Close files and unmount before reconfiguring. One static port backs the callbacks;
 * all card access must use the single storage task, outside interrupts.
 * @param spi Non-NULL handle for a ready SPI1, SPI2, or SPI3 master in mode 0,
 * with 8-bit, MSB-first, full-duplex transfers and software CS. TI mode and CRC are off.
 * @param cs_port Non-NULL GPIO port, already configured for an output.
 * @param cs_pin One GPIO pin mask, not a pin number.
 * @return true if configured at or below 400 kHz with callbacks registered;
 * false on invalid setup or HAL error.
 */
bool sd_spi_port_bind(SPI_HandleTypeDef *spi, GPIO_TypeDef *cs_port, uint16_t cs_pin);

#ifdef __cplusplus
}
#endif

#endif
