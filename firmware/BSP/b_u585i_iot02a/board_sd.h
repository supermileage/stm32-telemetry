/**
 * @file
 * @brief SD wiring and setup for the B-U585I-IOT02A board.
 */
#ifndef BOARD_SD_H
#define BOARD_SD_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Configure SPI1 with PE12 as CS and register the SD driver's hardware callbacks.
 * Failure removes any previously registered callbacks.
 * Called by storage_mount() in the storage task after CubeMX GPIO/SPI setup,
 * with files closed and the card unmounted. No card commands are sent here.
 * @return true if the interface is ready at or below 400 kHz; false on setup failure.
 */
bool board_sd_configure(void);

#ifdef __cplusplus
}
#endif

#endif
