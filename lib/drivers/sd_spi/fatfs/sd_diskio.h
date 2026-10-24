#ifndef SD_DISKIO_H
#define SD_DISKIO_H

/**
 * @file sd_diskio.h
 * @brief Register the SD SPI block driver with STM32Cube FatFs.
 *
 * Supports one card on drive 0. Configure the SD SPI port before mounting.
 * Registration does not initialize the card or mount a filesystem. Serialize
 * all adapter, FatFs, and SD driver calls in the same owning context.
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register the SD driver as FatFs drive 0.
 * Hardware must be configured first. FatFs initializes the card when mounting or accessing it.
 * @param[out] path Four-byte buffer receiving "0:/".
 * @return true if registered; false for invalid input, an occupied slot, or dispatcher failure.
 */
bool sd_diskio_register(char path[4]);

/**
 * @brief Unlink the driver after closing files and unmounting.
 * @param[in,out] path Registered drive path; cleared on success.
 * @return true if unlinked; false for invalid input, no registration, or dispatcher failure.
 */
bool sd_diskio_unregister(char path[4]);

#ifdef __cplusplus
}
#endif

#endif
