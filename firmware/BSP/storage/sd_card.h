#ifndef SD_CARD_H
#define SD_CARD_H

/**
 * @file
 * @brief Board SD configuration for SPI1 with chip select on PE12.
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bind the SD driver to SPI1 and PE12.
 *
 * Call from the logger task with files closed and the card unmounted.
 * CubeMX must configure mode 0 and a fixed clock of 400 kHz or lower.
 * This checks the configuration and binds callbacks; FatFs initializes the card.
 * @return true if bound; false on failure, clearing the old binding unless an
 *         operation is still acquired (in which case the binding is left alone).
 */
bool sd_card_configure(void);

#ifdef __cplusplus
}
#endif

#endif
