#ifndef SD_DISKIO_H
#define SD_DISKIO_H

/**
 * @file sd_diskio.h
 * @brief FatFs drive 0 registration for the SD card.
 * Logger task only. Configure SPI before registering.
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register the SD driver as FatFs drive 0.
 * Does not initialize the card or mount the filesystem.
 * @param[out] path Receives the drive path "0:/".
 * @return true if registered.
 */
bool sd_diskio_register(char path[4]);

/**
 * @brief Unregister the SD driver after closing files and unmounting.
 * @param[in,out] path Registered drive path. Cleared on success.
 * @return true if unregistered.
 */
bool sd_diskio_unregister(char path[4]);

#ifdef __cplusplus
}
#endif

#endif
