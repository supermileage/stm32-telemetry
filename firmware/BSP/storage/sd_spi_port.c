#include "sd_spi_port.h"
#include "sd_card.h"
#include "spi/spi_bus.h"
#include "main.h"
#include "spi.h"
#include <string.h>

static const uint32_t timeout_ms = 50U;
static const uint32_t maximum_sck_hz = 400000U;

typedef struct {
    spi_device_t device;
    SD_Card card;
    uint32_t sck_hz;
    bool bound;
    bool locked;
    bool failed;
} sd_spi_port_t;

static sd_spi_port_t port;

static bool transfer(sd_spi_port_t *p, const uint8_t *tx, uint8_t *rx, uint16_t length)
{
    // Byte and CS callbacks cannot return errors, so release reports them.
    if (!p->locked || p->failed || !spi_transfer(&p->device, tx, rx, length, timeout_ms)) {
        p->failed = true;
        return false;
    }
    return true;
}

static uint8_t exchange(void *context, uint8_t tx)
{
    sd_spi_port_t *p = context;
    uint8_t rx = 0xFFU;
    (void)transfer(p, &tx, &rx, 1U);
    return rx;
}

static int receive(void *context, uint8_t *rx, uint16_t length)
{
    sd_spi_port_t *p = context;
    uint8_t dummy[32];
    // Small chunks provide read clocks without a sector-sized scratch buffer.
    memset(dummy, 0xFF, sizeof(dummy));
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

static int send(void *context, const uint8_t *tx, uint16_t length)
{
    sd_spi_port_t *p = context;
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

static void select(void *context)
{
    sd_spi_port_t *p = context;
    if (!p->locked || !spi_select(&p->device, true)) {
        p->failed = true;
    }
}

static void deselect(void *context)
{
    sd_spi_port_t *p = context;
    if (!p->locked || !spi_select(&p->device, false)) {
        p->failed = true;
        return;
    }
    // Supply eight trailing clocks with CS high, as the library requires.
    (void)exchange(context, 0xFFU);
}

static int set_speed(void *context, uint8_t speed)
{
    const sd_spi_port_t *p = context;
    // Initialization and normal I/O both use the validated CubeMX clock.
    return (speed == SD_SPI_SPEED_LOW || speed == SD_SPI_SPEED_HIGH) &&
           p->sck_hz != 0U && p->sck_hz <= maximum_sck_hz ? SD_OK : SD_ERR;
}

static uint32_t clock_hz(void *context)
{
    const sd_spi_port_t *p = context;
    return p->sck_hz;
}

static uint32_t tick_ms(void *context)
{
    (void)context;
    return HAL_GetTick();
}

bool sd_card_configure(void)
{
    static spi_bus_t bus;

    if (port.locked) {
        return false;
    }
    port = (sd_spi_port_t){0};
    if (!spi_bus_init(&bus, &hspi1)) {
        return false;
    }
    // CFG1.MBR encodes divisors 2, 4, ... 256 as values 0, 1, ... 7.
    uint32_t mbr = (hspi1.Init.BaudRatePrescaler & SPI_CFG1_MBR) >> SPI_CFG1_MBR_Pos;
    uint32_t sck_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI1) / (2U << mbr);
    if (sck_hz == 0U || sck_hz > maximum_sck_hz ||
        hspi1.Init.Mode != SPI_MODE_MASTER ||
        hspi1.Init.Direction != SPI_DIRECTION_2LINES ||
        hspi1.Init.DataSize != SPI_DATASIZE_8BIT ||
        hspi1.Init.FirstBit != SPI_FIRSTBIT_MSB ||
        hspi1.Init.NSS != SPI_NSS_SOFT ||
        hspi1.Init.CLKPolarity != SPI_POLARITY_LOW ||
        hspi1.Init.CLKPhase != SPI_PHASE_1EDGE) {
        return false;
    }
    port.device = (spi_device_t){&bus, SD_CS_GPIO_Port, SD_CS_Pin};
    port.sck_hz = sck_hz;
    if (!spi_acquire(&port.device, pdMS_TO_TICKS(timeout_ms))) {
        return false;
    }
    spi_end(&port.device);
    SD_IO io = {0};
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

SD_Card *sd_spi_port_acquire(void)
{
    // SD needs CS-high clocks while retaining the lock across the whole operation.
    if (!port.bound || port.locked || !spi_acquire(&port.device, pdMS_TO_TICKS(timeout_ms))) {
        return NULL;
    }
    port.locked = true;
    port.failed = false;
    return &port.card;
}

bool sd_spi_port_release(void)
{
    if (!port.locked) {
        return false;
    }
    spi_end(&port.device);
    port.locked = false;
    if (port.failed) {
        SD_DeInit_Card(&port.card);
    }
    return !port.failed;
}

bool sd_spi_port_is_ready(void)
{
    return port.bound && !port.failed && SD_Get_Info_Card(&port.card)->initialized != 0U;
}
