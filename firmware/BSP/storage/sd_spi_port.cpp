#include "sd_spi_port.hpp"
#include "sd_spi.h"

namespace {

constexpr uint32_t timeout_ms = 50U;

struct {
    spi_device_t device;
    bool ready;
    bool selected;
} port{};

bool exchange(uint8_t tx, uint8_t *rx)
{
    if (!port.ready || rx == nullptr) {
        return false;
    }
    // When CS is high, borrow the bus just long enough to send dummy clocks.
    if (!port.selected && !spi_acquire(&port.device, pdMS_TO_TICKS(timeout_ms))) {
        return false;
    }

    bool ok = spi_exchange(&port.device, &tx, rx, 1U, timeout_ms);
    if (!port.selected) {
        spi_release(&port.device);
    }
    return ok;
}

void chip_select(bool active)
{
    if (!active) {
        if (port.selected) {
            spi_release(&port.device);
            port.selected = false;
        }
        return;
    }
    if (!port.ready || port.selected) {
        return;
    }
    if (!spi_acquire(&port.device, pdMS_TO_TICKS(timeout_ms))) {
        // The callback cannot return an error; make the next exchange fail.
        port.ready = false;
        return;
    }
    port.selected = spi_select(&port.device, true);
    if (!port.selected) {
        spi_release(&port.device);
        port.ready = false;
    }
}

void clear_binding()
{
    chip_select(false);
    port = {};
    (void)sd_spi_bind(nullptr);
}

} // namespace

bool sd_spi_port_bind(spi_bus_t *bus, GPIO_TypeDef *cs_port, uint16_t cs_pin)
{
    if (port.selected || bus == nullptr || bus->handle == nullptr ||
        bus->handle->Init.CLKPolarity != SPI_POLARITY_LOW ||
        bus->handle->Init.CLKPhase != SPI_PHASE_1EDGE) {
        clear_binding();
        return false;
    }

    port = {};
    port.device = {bus, cs_port, cs_pin};
    if (!spi_acquire(&port.device, pdMS_TO_TICKS(timeout_ms))) {
        clear_binding();
        return false;
    }
    spi_release(&port.device);
    port.ready = true;
    const sd_spi_port_t callbacks{exchange, chip_select, HAL_GetTick, HAL_Delay};
    if (!sd_spi_bind(&callbacks)) {
        clear_binding();
        return false;
    }
    return true;
}
