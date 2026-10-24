#pragma once

#include "spi/spi_bus.h"

/**
 * @brief Connect the SD driver to a dedicated SPI bus.
 *
 * Use mode 0, a fixed clock of 400 kHz or lower, and a CS output initially high.
 * Call from the storage task; close files and unmount before rebinding.
 * @param bus Initialized, long-lived bus; nullptr clears the binding.
 * @param cs_port Chip-select GPIO port.
 * @param cs_pin Chip-select GPIO_PIN_x mask.
 * @return true if bound; false on invalid setup or an unavailable bus. Failure clears the binding.
 */
bool sd_spi_port_bind(spi_bus_t *bus, GPIO_TypeDef *cs_port, uint16_t cs_pin);
