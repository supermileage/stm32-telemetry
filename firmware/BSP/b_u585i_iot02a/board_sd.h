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
 * Call after CubeMX GPIO/SPI setup, with files closed and the card unmounted.
 * This only configures the interface; initialize the card later from the storage task.
 * @return true if the interface is ready at or below 400 kHz; false on setup failure.
 */
bool board_sd_configure(void);

#ifdef __cplusplus
}

namespace bsp {

/**
 * C++ entry point with the same setup and call requirements as board_sd_configure().
 * @return true on successful setup; false on failure, with the callbacks removed.
 */
bool configureSd() noexcept;

} // namespace bsp
#endif

#endif
