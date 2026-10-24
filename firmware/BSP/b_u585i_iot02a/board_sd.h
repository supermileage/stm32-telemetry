#ifndef BOARD_SD_H
#define BOARD_SD_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bind the SD driver to SPI1 and PE12.
 *
 * Call from the storage task with files closed and the card unmounted.
 * CubeMX must configure a fixed clock of 400 kHz or lower.
 * @return true if bound; false on failure, clearing the old binding.
 */
bool board_sd_configure(void);

#ifdef __cplusplus
}
#endif

#endif
