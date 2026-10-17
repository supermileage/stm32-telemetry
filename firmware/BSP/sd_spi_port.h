/**
 * @file
 * @brief STM32 HAL SPI and chip-select callbacks for the SD driver.
 */
#ifndef SD_SPI_PORT_H
#define SD_SPI_PORT_H

#include "stm32u5xx_hal.h"

#include <stdbool.h>

#ifdef __cplusplus
#include <cstdint>

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

/** Board hardware setup and the SD driver's SPI port. */
namespace bsp {

/**
 * Non-owning access to one dedicated SPI bus and active-low CS output.
 * The SPI handle and GPIO port must outlive this object. Construction does not
 * touch hardware; configure() performs setup. Calls are not synchronized.
 */
class SdSpiPort {
public:
    /** Leave hardware untouched until configure() is called. */
    SdSpiPort() = default;
    /** Each port represents one hardware connection, so copying is disabled. */
    SdSpiPort(const SdSpiPort&) = delete;
    /** Assignment is disabled for the same reason as copying. */
    SdSpiPort& operator=(const SdSpiPort&) = delete;

    /**
     * Configure an initialized SPI bus and CS output at or below 400 kHz.
     * Call with the card deselected and all files closed and unmounted.
     * @param spi Ready SPI1/2/3 handle with the settings required by sd_spi_port_bind().
     * @param csPort GPIO port already configured for an output.
     * @param csPin One GPIO pin mask, not a pin number.
     * @return true on success; false for invalid setup, an active card, or a HAL error.
     */
    bool configure(SPI_HandleTypeDef& spi, GPIO_TypeDef& csPort,
                   std::uint16_t csPin) noexcept;

    /**
     * Exchange one byte with a 50 ms timeout; the SD driver controls CS.
     * @param tx Byte to transmit; use 0xFF when only reading from the card.
     * @param[out] rx Non-NULL destination for the received byte, valid on success.
     * @return false for invalid arguments, an unready bus, or a HAL error; true otherwise.
     */
    bool exchange(std::uint8_t tx, std::uint8_t* rx) noexcept;

    /**
     * Drive CS low when active, high otherwise. An unconfigured port does nothing.
     * Selecting an unready bus is ignored; deselecting still releases a configured pin.
     * @param active true to select the card, false to release it.
     */
    void select(bool active) noexcept;

    /**
     * Choose the fastest divider that keeps the SPI clock at or below hz.
     * Uses the current peripheral clock and reinitializes SPI with that divider.
     * @param hz Nonzero upper clock limit in Hz, attainable with a /2 to /256 divider.
     * @return true on success; false for an unready bus, selected card, unattainable
     * limit, or HAL error. A HAL init failure leaves transfers disabled.
     */
    bool setMaxFrequency(std::uint32_t hz) noexcept;

private:
    SPI_HandleTypeDef* spi_{};
    GPIO_TypeDef* csPort_{};
    std::uint16_t csPin_{};
    bool ready_{};
    bool selected_{};
};

} // namespace bsp
#endif

#endif
