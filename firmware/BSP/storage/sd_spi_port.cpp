#include "sd_spi_port.hpp"
#include "sd_card.h"
#include "spi/spi_bus.h"
#include "main.h"
#include "spi.h"
#include <cstring>

namespace {
constexpr uint32_t timeout_ms = 50U;
constexpr uint32_t maximum_sck_hz = 400000U;

struct Port {
    spi_device_t device;
    SD_Card card;
    uint32_t sck_hz;
    bool bound;
    bool locked;
    bool failed;
} port{};

bool transfer(Port &p, const uint8_t *tx, uint8_t *rx, uint16_t length)
{
    // Byte and CS callbacks cannot return errors, so release reports them.
    if (!p.locked || p.failed || !spi_exchange(&p.device, tx, rx, length, timeout_ms)) {
        p.failed = true;
        return false;
    }
    return true;
}

uint8_t exchange(void *context, uint8_t tx)
{
    auto &p = *static_cast<Port *>(context);
    uint8_t rx = 0xFFU;
    (void)transfer(p, &tx, &rx, 1U);
    return rx;
}

int receive(void *context, uint8_t *rx, uint16_t length)
{
    auto &p = *static_cast<Port *>(context);
    uint8_t dummy[32];
    // Small chunks provide read clocks without a sector-sized scratch buffer.
    std::memset(dummy, 0xFF, sizeof(dummy));
    while (length != 0U) {
        uint16_t chunk = length < sizeof(dummy) ? length : sizeof(dummy);
        if (!transfer(p, dummy, rx, chunk)) {
            return SD_ERR;
        }
        rx += chunk;
        length -= chunk;
    }
    return SD_OK;
}

int send(void *context, const uint8_t *tx, uint16_t length)
{
    auto &p = *static_cast<Port *>(context);
    uint8_t discarded[32];
    while (length != 0U) {
        uint16_t chunk = length < sizeof(discarded) ? length : sizeof(discarded);
        if (!transfer(p, tx, discarded, chunk)) {
            return SD_ERR;
        }
        tx += chunk;
        length -= chunk;
    }
    return SD_OK;
}

void select(void *context)
{
    auto &p = *static_cast<Port *>(context);
    if (!p.locked || !spi_select(&p.device, true)) {
        p.failed = true;
    }
}

void deselect(void *context)
{
    auto &p = *static_cast<Port *>(context);
    if (!p.locked || !spi_select(&p.device, false)) {
        p.failed = true;
        return;
    }
    // Supply eight trailing clocks with CS high, as the library requires.
    (void)exchange(context, 0xFFU);
}

int set_speed(void *context, uint8_t speed)
{
    const auto &p = *static_cast<Port *>(context);
    // Initialization and normal I/O both use the validated CubeMX clock.
    return (speed == SD_SPI_SPEED_LOW || speed == SD_SPI_SPEED_HIGH) &&
           p.sck_hz != 0U && p.sck_hz <= maximum_sck_hz ? SD_OK : SD_ERR;
}

uint32_t clock_hz(void *context)
{
    return static_cast<Port *>(context)->sck_hz;
}

uint32_t tick_ms(void *)
{
    return HAL_GetTick();
}
} // namespace

bool sd_card_configure(void)
{
    static spi_bus_t bus{};

    if (port.locked) {
        return false;
    }
    port = {};
    if (!spi_bus_init(&bus, &hspi1)) {
        return false;
    }
    // CFG1.MBR encodes divisors 2, 4, ... 256 as values 0, 1, ... 7.
    uint32_t mbr = (hspi1.Init.BaudRatePrescaler & SPI_CFG1_MBR) >> SPI_CFG1_MBR_Pos;
    uint32_t sck_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI1) / (2U << mbr);
    if (sck_hz == 0U || sck_hz > maximum_sck_hz ||
        hspi1.Init.CLKPolarity != SPI_POLARITY_LOW ||
        hspi1.Init.CLKPhase != SPI_PHASE_1EDGE) {
        return false;
    }
    port.device = {&bus, SD_CS_GPIO_Port, SD_CS_Pin};
    port.sck_hz = sck_hz;
    if (!spi_acquire(&port.device, pdMS_TO_TICKS(timeout_ms))) {
        return false;
    }
    spi_release(&port.device);
    SD_IO io{};
    io.context = &port;
    io.spi_byte = exchange;
    io.receive = receive;
    io.send = send;
    io.cs_low = select;
    io.cs_high = deselect;
    io.set_speed = set_speed;
    io.get_sck_hz = clock_hz;
    io.tick_ms = tick_ms;
    io.crc_check = 1U;
    port.bound = SD_Card_BindIO(&port.card, &io) == SD_OK;
    return port.bound;
}

SD_Card *sd_spi_port_acquire()
{
    if (!port.bound || port.locked || !spi_acquire(&port.device, pdMS_TO_TICKS(timeout_ms))) {
        return nullptr;
    }
    port.locked = true;
    port.failed = false;
    return &port.card;
}

bool sd_spi_port_release()
{
    if (!port.locked) {
        return false;
    }
    spi_release(&port.device);
    port.locked = false;
    if (port.failed) {
        SD_DeInit_Card(&port.card);
    }
    return !port.failed;
}

bool sd_spi_port_is_ready()
{
    return port.bound && !port.failed && SD_Get_Info_Card(&port.card)->initialized != 0U;
}
