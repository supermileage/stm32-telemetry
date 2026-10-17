/**
 * @file
 * @brief STM32 HAL transfers and hardware callback registration for the SD driver.
 */
#include "sd_spi_port.h"
#include "sd_spi.h"

#include <cstddef>

namespace bsp {

bool SdSpiPort::configure(SPI_HandleTypeDef& spi, GPIO_TypeDef& csPort,
                         std::uint16_t csPin) noexcept
{
    if (selected_) {
        return false;
    }

    spi_ = nullptr;
    csPort_ = nullptr;
    csPin_ = 0;
    ready_ = false;

    if (csPin == 0 || (csPin & (csPin - 1U)) != 0 ||
        (spi.Instance != SPI1 && spi.Instance != SPI2 && spi.Instance != SPI3) ||
        spi.State != HAL_SPI_STATE_READY ||
        spi.Init.Mode != SPI_MODE_MASTER || spi.Init.Direction != SPI_DIRECTION_2LINES ||
        spi.Init.DataSize != SPI_DATASIZE_8BIT || spi.Init.FirstBit != SPI_FIRSTBIT_MSB ||
        spi.Init.CLKPolarity != SPI_POLARITY_LOW || spi.Init.CLKPhase != SPI_PHASE_1EDGE ||
        spi.Init.NSS != SPI_NSS_SOFT || spi.Init.TIMode != SPI_TIMODE_DISABLE ||
        spi.Init.CRCCalculation != SPI_CRCCALCULATION_DISABLE) {
        return false;
    }

    spi_ = &spi;
    csPort_ = &csPort;
    csPin_ = csPin;
    select(false);
    if (!setMaxFrequency(400000U)) {
        spi_ = nullptr;
        csPort_ = nullptr;
        csPin_ = 0;
        return false;
    }
    return true;
}

bool SdSpiPort::exchange(std::uint8_t tx, std::uint8_t* rx) noexcept
{
    if (spi_ == nullptr || !ready_ || spi_->State != HAL_SPI_STATE_READY || rx == nullptr) {
        return false;
    }
    return HAL_SPI_TransmitReceive(spi_, &tx, rx, 1U, 50U) == HAL_OK;
}

void SdSpiPort::select(bool active) noexcept
{
    if (csPort_ == nullptr || (active && !ready_)) {
        return;
    }
    HAL_GPIO_WritePin(csPort_, csPin_, active ? GPIO_PIN_RESET : GPIO_PIN_SET);
    selected_ = active;
}

bool SdSpiPort::setMaxFrequency(std::uint32_t hz) noexcept
{
    if (spi_ == nullptr || selected_ || hz == 0 || spi_->State != HAL_SPI_STATE_READY) {
        return false;
    }

    constexpr std::uint32_t prescalers[] = {
        SPI_BAUDRATEPRESCALER_2, SPI_BAUDRATEPRESCALER_4,
        SPI_BAUDRATEPRESCALER_8, SPI_BAUDRATEPRESCALER_16,
        SPI_BAUDRATEPRESCALER_32, SPI_BAUDRATEPRESCALER_64,
        SPI_BAUDRATEPRESCALER_128, SPI_BAUDRATEPRESCALER_256
    };
    const std::uint64_t source =
        spi_->Instance == SPI1 ? RCC_PERIPHCLK_SPI1 :
        spi_->Instance == SPI2 ? RCC_PERIPHCLK_SPI2 : RCC_PERIPHCLK_SPI3;
    const auto clockHz = HAL_RCCEx_GetPeriphCLKFreq(source);
    if (clockHz == 0U) {
        return false;
    }
    for (std::size_t i = 0; i < sizeof(prescalers) / sizeof(prescalers[0]); ++i) {
        const auto divisor = 2U << i;
        // Compare without rounding the SPI clock down or overflowing 32 bits.
        if (static_cast<std::uint64_t>(clockHz) <= static_cast<std::uint64_t>(hz) * divisor) {
            spi_->Init.BaudRatePrescaler = prescalers[i];
            spi_->Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
            // Keep SCK/MOSI driven while HAL disables SPI between transfers.
            spi_->Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
            ready_ = false;
            if (HAL_SPI_Init(spi_) != HAL_OK) {
                return false;
            }
            ready_ = true;
            return true;
        }
    }
    return false;
}

} // namespace bsp

namespace {

bsp::SdSpiPort port;

bool exchange(std::uint8_t tx, std::uint8_t* rx)
{
    return port.exchange(tx, rx);
}

bool setSpeed(std::uint32_t maximumHz)
{
    return port.setMaxFrequency(maximumHz);
}

void select(bool active)
{
    port.select(active);
}

} // namespace

bool sd_spi_port_bind(SPI_HandleTypeDef* spi, GPIO_TypeDef* cs_port, uint16_t cs_pin)
{
    if (spi == nullptr || cs_port == nullptr || !port.configure(*spi, *cs_port, cs_pin)) {
        // If setup is rejected, release the previous card's CS pin too.
        port.select(false);
        (void)sd_spi_bind(nullptr);
        return false;
    }
    const sd_spi_port_t callbacks{exchange, setSpeed, select, HAL_GetTick, HAL_Delay};
    return sd_spi_bind(&callbacks);
}
