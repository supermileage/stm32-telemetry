#pragma once
#include "SD_reader.h"

/**
 * @file
 * @brief SD SPI access for the FatFs adapter.
 * Logger task only. Configure before use and pair acquire with release.
 */

/**
 * @brief Lock SPI1 with CS high for one SD operation.
 * @return Card handle valid until release, or nullptr on failure.
 */
SD_Card *sd_spi_port_acquire();

/**
 * @brief Deselect the card and unlock SPI1.
 * I/O errors require card initialization before reuse.
 * @return true if the operation completed without callback errors.
 */
bool sd_spi_port_release();

/**
 * @brief Check cached card status without reading hardware.
 * @return true if initialized with no recorded I/O failure.
 */
bool sd_spi_port_is_ready();
