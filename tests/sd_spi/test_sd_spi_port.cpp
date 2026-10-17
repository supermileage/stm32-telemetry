#include "sd_spi_port.h"
#include "board_sd.h"
#include "main.h"
#include "spi.h"
#include "sd_spi.h"

#include <cassert>
#include <cstdio>
#include <type_traits>

SPI_TypeDef mock_spi1{}, mock_spi2{}, mock_spi3{};
GPIO_TypeDef gpio{};
SPI_HandleTypeDef hspi1{};

namespace {

std::uint32_t clockHz = 16000000U, tick, lastDelay, lastTimeout;
std::uint64_t lastClock;
unsigned initCalls, transferCalls, gpioCalls;
std::uint16_t lastCount, lastPin;
GPIO_TypeDef* lastPort;
GPIO_PinState lastCs = GPIO_PIN_SET;
HAL_StatusTypeDef initResult = HAL_OK, transferResult = HAL_OK;
bool absentCard;

void resetHardware()
{
    clockHz = 16000000U;
    tick = lastDelay = lastTimeout = 0;
    lastClock = 0;
    initCalls = transferCalls = gpioCalls = 0;
    lastCount = lastPin = 0;
    lastPort = nullptr;
    lastCs = GPIO_PIN_SET;
    initResult = transferResult = HAL_OK;
    absentCard = false;
}

SPI_HandleTypeDef validSpi(SPI_TypeDef* instance = SPI1)
{
    SPI_HandleTypeDef spi{};
    spi.Instance = instance;
    spi.State = HAL_SPI_STATE_READY;
    spi.Init.Mode = SPI_MODE_MASTER;
    spi.Init.Direction = SPI_DIRECTION_2LINES;
    spi.Init.DataSize = SPI_DATASIZE_8BIT;
    spi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    spi.Init.CLKPolarity = SPI_POLARITY_LOW;
    spi.Init.CLKPhase = SPI_PHASE_1EDGE;
    spi.Init.NSS = SPI_NSS_SOFT;
    spi.Init.TIMode = SPI_TIMODE_DISABLE;
    spi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    return spi;
}

void testConfiguration()
{
    resetHardware();
    bsp::SdSpiPort port;
    static_assert(!std::is_copy_constructible_v<bsp::SdSpiPort>);
    static_assert(!std::is_move_constructible_v<bsp::SdSpiPort>);
    std::uint8_t tx = 0xFF, rx{};
    assert(!port.exchange(tx, &rx));
    assert(!port.setMaxFrequency(400000));
    port.select(true);
    assert(gpioCalls == 0 && initCalls == 0 && transferCalls == 0);

    auto spi = validSpi();
    assert(!port.configure(spi, gpio, 0));
    assert(!port.configure(spi, gpio, 3));
    spi.State = HAL_SPI_STATE_RESET;
    assert(!port.configure(spi, gpio, SD_CS_Pin));
    spi = validSpi(nullptr);
    assert(!port.configure(spi, gpio, SD_CS_Pin));
    SPI_TypeDef unsupported{};
    spi.Instance = &unsupported;
    assert(!port.configure(spi, gpio, SD_CS_Pin));

    constexpr std::uint32_t SPI_InitTypeDef::* fields[] = {
        &SPI_InitTypeDef::Mode, &SPI_InitTypeDef::Direction, &SPI_InitTypeDef::DataSize,
        &SPI_InitTypeDef::FirstBit, &SPI_InitTypeDef::CLKPolarity, &SPI_InitTypeDef::CLKPhase,
        &SPI_InitTypeDef::NSS, &SPI_InitTypeDef::TIMode, &SPI_InitTypeDef::CRCCalculation
    };
    for (auto field : fields) {
        spi = validSpi();
        spi.Init.*field = UINT32_MAX;
        assert(!port.configure(spi, gpio, SD_CS_Pin));
    }
    assert(initCalls == 0 && gpioCalls == 0);
    spi = validSpi();
    assert(port.configure(spi, gpio, SD_CS_Pin));
    assert(spi.Init.BaudRatePrescaler == SPI_BAUDRATEPRESCALER_64);
    assert(lastPort == &gpio && lastPin == SD_CS_Pin && lastCs == GPIO_PIN_SET);
    assert(transferCalls == 0); // Configuration sends no card commands.

    port.select(true);
    assert(lastCs == GPIO_PIN_RESET);
    const auto before = initCalls;
    assert(!port.setMaxFrequency(4000000));
    assert(!port.configure(spi, gpio, SD_CS_Pin));
    assert(initCalls == before && lastCs == GPIO_PIN_RESET);
    port.select(false);
    assert(lastCs == GPIO_PIN_SET);

    clockHz = 160000000U; // No divider can meet the 400 kHz startup limit.
    assert(!port.configure(spi, gpio, SD_CS_Pin));
    assert(!port.exchange(tx, &rx));
    const auto pinsBefore = gpioCalls;
    port.select(true);
    assert(gpioCalls == pinsBefore && lastCs == GPIO_PIN_SET);
}

void testClockLimits()
{
    resetHardware();
    bsp::SdSpiPort port;
    auto spi = validSpi();
    assert(port.configure(spi, gpio, SD_CS_Pin));
    assert(port.setMaxFrequency(4000000));
    assert(spi.Init.BaudRatePrescaler == SPI_BAUDRATEPRESCALER_4);
    assert(port.setMaxFrequency(3000000));
    assert(spi.Init.BaudRatePrescaler == SPI_BAUDRATEPRESCALER_8);
    assert(port.setMaxFrequency(62500));
    assert(spi.Init.BaudRatePrescaler == SPI_BAUDRATEPRESCALER_256);
    const auto before = initCalls;
    assert(!port.setMaxFrequency(62499));
    assert(!port.setMaxFrequency(0));
    assert(initCalls == before);
    clockHz = 0;
    assert(!port.setMaxFrequency(400000));
    clockHz = 16000001;
    assert(port.setMaxFrequency(250000));
    assert(spi.Init.BaudRatePrescaler == SPI_BAUDRATEPRESCALER_128);
    assert(port.setMaxFrequency(UINT32_MAX));
    assert(spi.Init.BaudRatePrescaler == SPI_BAUDRATEPRESCALER_2);

    clockHz = 16000000;
    spi = validSpi(SPI2);
    assert(port.configure(spi, gpio, SD_CS_Pin));
    assert(lastClock == RCC_PERIPHCLK_SPI2);
    spi = validSpi(SPI3);
    assert(port.configure(spi, gpio, SD_CS_Pin));
    assert(lastClock == RCC_PERIPHCLK_SPI3);
}

void testTransfersAndFailures()
{
    resetHardware();
    bsp::SdSpiPort port;
    auto spi = validSpi();
    assert(port.configure(spi, gpio, SD_CS_Pin));
    std::uint8_t rx{};
    assert(!port.exchange(0xFF, nullptr));
    spi.State = HAL_SPI_STATE_BUSY;
    assert(!port.exchange(0xFF, &rx));
    assert(!port.setMaxFrequency(4000000));
    spi.State = HAL_SPI_STATE_READY;
    assert(transferCalls == 0);
    port.select(true);
    const auto pinsBefore = gpioCalls;
    assert(port.exchange(0xFF, &rx));
    assert(lastCount == 1 && lastTimeout == 50 && rx == 0x5A);
    assert(gpioCalls == pinsBefore && lastCs == GPIO_PIN_RESET);
    port.select(false);

    assert(port.exchange(0x12, &rx) && rx == 0xB7);
    constexpr HAL_StatusTypeDef failures[] = {HAL_ERROR, HAL_BUSY, HAL_TIMEOUT};
    for (auto error : failures) {
        transferResult = error;
        assert(!port.exchange(0xFF, &rx));
    }
    transferResult = HAL_OK;
    initResult = HAL_ERROR;
    assert(!port.setMaxFrequency(4000000));
    const auto callsBefore = transferCalls;
    assert(!port.exchange(0xFF, &rx));
    assert(transferCalls == callsBefore);
    initResult = HAL_OK;
    assert(port.configure(spi, gpio, SD_CS_Pin));
}

void testBoardBinding()
{
    resetHardware();
    hspi1 = validSpi();
    assert(bsp::configureSd());
    assert(lastClock == RCC_PERIPHCLK_SPI1 && lastPin == SD_CS_Pin && lastPort == &gpio);
    assert(!sd_spi_is_ready() && transferCalls == 0);
    assert(board_sd_configure()); // The C storage entry point uses the same BSP.

    // An absent card must time out through the real driver and C++ callbacks.
    absentCard = true;
    tick = UINT32_MAX - 20U;
    assert(sd_spi_initialize() == SD_NOT_READY);
    assert(transferCalls > 10 && lastCs == GPIO_PIN_SET);
    assert(lastDelay == 10 && tick < 100U); // HAL timing callbacks cross wraparound.
    assert(lastCount == 1 && lastTimeout == 50);

    sd_spi_diagnostics_t before{}, after{};
    sd_spi_get_diagnostics(&before);
    transferResult = HAL_ERROR;
    assert(sd_spi_initialize() == SD_ERROR && lastCs == GPIO_PIN_SET);
    sd_spi_get_diagnostics(&after);
    assert(after.transfer_failures == before.transfer_failures + 1U);

    // Failed reconfiguration must clear the driver's previous callback binding.
    initResult = HAL_ERROR;
    assert(!bsp::configureSd());
    const auto beforeFailure = transferCalls;
    assert(sd_spi_initialize() == SD_NOT_READY && transferCalls == beforeFailure);
    assert(lastCs == GPIO_PIN_SET);

    initResult = HAL_OK;
    assert(bsp::configureSd());
    assert(!sd_spi_port_bind(nullptr, &gpio, SD_CS_Pin));
    assert(sd_spi_initialize() == SD_NOT_READY && transferCalls == beforeFailure);
    assert(lastCs == GPIO_PIN_SET);

    assert(board_sd_configure());
    assert(!sd_spi_port_bind(&hspi1, nullptr, SD_CS_Pin));
    assert(sd_spi_initialize() == SD_NOT_READY && transferCalls == beforeFailure);

    assert(board_sd_configure());
    assert(!sd_spi_port_bind(&hspi1, &gpio, 3U));
    assert(sd_spi_initialize() == SD_NOT_READY && transferCalls == beforeFailure);
    assert(board_sd_configure()); // Recover after a rejected bind.
}
} // namespace

HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef* spi)
{
    ++initCalls;
    assert(spi->Init.NSSPMode == SPI_NSS_PULSE_DISABLE);
    assert(spi->Init.MasterKeepIOState == SPI_MASTER_KEEP_IO_STATE_ENABLE);
    return initResult;
}

HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef*, const std::uint8_t* tx,
                                        std::uint8_t* rx, std::uint16_t count,
                                        std::uint32_t timeout)
{
    ++transferCalls;
    ++tick;
    lastCount = count;
    lastTimeout = timeout;
    assert(timeout != 0 && timeout != UINT32_MAX);
    if (transferResult == HAL_OK) {
        for (std::uint32_t i = 0; i < count; ++i) {
            rx[i] = absentCard ? 0xFF : tx[i] ^ 0xA5U;
        }
    }
    return transferResult;
}

void HAL_GPIO_WritePin(GPIO_TypeDef* port, std::uint16_t pin, GPIO_PinState state)
{
    ++gpioCalls;
    lastPort = port;
    lastPin = pin;
    lastCs = state;
}

std::uint32_t HAL_RCCEx_GetPeriphCLKFreq(std::uint64_t clock)
{
    lastClock = clock;
    return clockHz;
}

std::uint32_t HAL_GetTick()
{
    return tick;
}

void HAL_Delay(std::uint32_t ms)
{
    lastDelay = ms;
    tick += ms;
}

int main()
{
    testConfiguration();
    testClockLimits();
    testTransfersAndFailures();
    testBoardBinding();
    std::puts("C++ SPI port passed: configuration, CS, clock limits, byte exchange, and real SD driver callbacks.");
}
