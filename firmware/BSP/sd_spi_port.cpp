/**
 * @file
 * @brief STM32 HAL transfers and hardware callback registration for the SD driver.
 */
#include "sd_spi_port.h"
#include "sd_spi.h"

#include <cstddef>
#include <cstdint>

namespace {

struct {
    SPI_HandleTypeDef* spi;
    GPIO_TypeDef* cs_port;
    uint16_t cs_pin;
    bool ready;
    bool selected;
} port{};

bool exchange(uint8_t tx, uint8_t* rx)
{
    if (port.spi == nullptr || !port.ready ||
        port.spi->State != HAL_SPI_STATE_READY || rx == nullptr) {
        return false;
    }
    return HAL_SPI_TransmitReceive(port.spi, &tx, rx, 1U, 50U) == HAL_OK;
}

void select(bool active)
{
    if (port.cs_port == nullptr || (active && !port.ready)) {
        return;
    }
    HAL_GPIO_WritePin(port.cs_port, port.cs_pin, active ? GPIO_PIN_RESET : GPIO_PIN_SET);
    port.selected = active;
}

bool set_speed(uint32_t maximum_hz)
{
    if (port.spi == nullptr || port.selected || maximum_hz == 0U ||
        port.spi->State != HAL_SPI_STATE_READY) {
        return false;
    }

    constexpr uint32_t prescalers[] = {
        SPI_BAUDRATEPRESCALER_2, SPI_BAUDRATEPRESCALER_4,
        SPI_BAUDRATEPRESCALER_8, SPI_BAUDRATEPRESCALER_16,
        SPI_BAUDRATEPRESCALER_32, SPI_BAUDRATEPRESCALER_64,
        SPI_BAUDRATEPRESCALER_128, SPI_BAUDRATEPRESCALER_256
    };
    const uint64_t source =
        port.spi->Instance == SPI1 ? RCC_PERIPHCLK_SPI1 :
        port.spi->Instance == SPI2 ? RCC_PERIPHCLK_SPI2 : RCC_PERIPHCLK_SPI3;
    const auto clock_hz = HAL_RCCEx_GetPeriphCLKFreq(source);
    if (clock_hz == 0U) {
        return false;
    }
    for (std::size_t i = 0; i < sizeof(prescalers) / sizeof(prescalers[0]); ++i) {
        const auto divisor = 2U << i;
        // Compare without rounding the SPI clock down or overflowing 32 bits.
        if (static_cast<uint64_t>(clock_hz) <= static_cast<uint64_t>(maximum_hz) * divisor) {
            port.spi->Init.BaudRatePrescaler = prescalers[i];
            port.spi->Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
            // Keep SCK/MOSI driven while HAL disables SPI between transfers.
            port.spi->Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
            port.ready = false;
            if (HAL_SPI_Init(port.spi) != HAL_OK) {
                return false;
            }
            port.ready = true;
            return true;
        }
    }
    return false;
}

void clear_binding()
{
    select(false);
    port = {};
    (void)sd_spi_bind(nullptr);
}

} // namespace

bool sd_spi_port_bind(SPI_HandleTypeDef* spi, GPIO_TypeDef* cs_port, uint16_t cs_pin)
{
    if (port.selected || spi == nullptr || cs_port == nullptr ||
        cs_pin == 0U || (cs_pin & (cs_pin - 1U)) != 0U ||
        (spi->Instance != SPI1 && spi->Instance != SPI2 && spi->Instance != SPI3) ||
        spi->State != HAL_SPI_STATE_READY ||
        spi->Init.Mode != SPI_MODE_MASTER || spi->Init.Direction != SPI_DIRECTION_2LINES ||
        spi->Init.DataSize != SPI_DATASIZE_8BIT || spi->Init.FirstBit != SPI_FIRSTBIT_MSB ||
        spi->Init.CLKPolarity != SPI_POLARITY_LOW || spi->Init.CLKPhase != SPI_PHASE_1EDGE ||
        spi->Init.NSS != SPI_NSS_SOFT || spi->Init.TIMode != SPI_TIMODE_DISABLE ||
        spi->Init.CRCCalculation != SPI_CRCCALCULATION_DISABLE) {
        clear_binding();
        return false;
    }

    select(false);
    port = {spi, cs_port, cs_pin, false, false};
    select(false);
    if (!set_speed(400000U)) {
        clear_binding();
        return false;
    }
    const sd_spi_port_t callbacks{exchange, set_speed, select, HAL_GetTick, HAL_Delay};
    return sd_spi_bind(&callbacks);
}
