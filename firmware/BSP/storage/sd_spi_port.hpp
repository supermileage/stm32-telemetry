#pragma once
#include "SD_reader.h"

/**
 * @file
 * @brief Internal SD port interface used by the FatFs adapter.
 *
 * The logger task owns all calls. Configure through sd_card_configure() before
 * use; keep each library operation between acquire and release, even on failure.
 */

/**
 * @brief Lock SPI1 for one complete SD library operation, leaving CS high.
 * @return Card handle valid until release, or nullptr if unconfigured, already
 *         acquired, called outside task context, or the 50 ms lock wait expires.
 */
SD_Card *sd_spi_port_acquire();

/**
 * @brief Deselect and unlock SPI1 after a library operation.
 * Callback failures invalidate the card until it is initialized again.
 * @return false if not acquired or a hardware callback failed; true otherwise.
 */
bool sd_spi_port_release();

/**
 * @brief Read cached initialization state without accessing the card.
 * @return true after successful initialization with no later I/O failure.
 * Physical removal is detected by subsequent I/O, not by this check.
 */
bool sd_spi_port_is_ready();
