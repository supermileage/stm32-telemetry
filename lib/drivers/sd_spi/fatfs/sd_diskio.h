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
 * @brief Register the SD SPI driver as the first attached FatFs driver.
 * @param path Receives the drive path ("0:/"); needs four bytes.
 * @return true on success; false for NULL, an existing registration, another
 *         attached driver, or a dispatcher registration failure.
 * @note FatFs initializes the card when the volume is mounted immediately or
 *       first accessed. This function does not configure board hardware.
 */
bool sd_diskio_register(char path[4]);

/**
 * @brief Unlink the SD driver after closing files and unmounting the volume.
 * @param path Drive path returned by sd_diskio_register(); cleared on success.
 * @return true on success; false for NULL, no registration, an invalid drive
 *         prefix, or a dispatcher unlink failure.
 * @note Does not close files, unmount, or clear the SD driver's ready state.
 */
bool sd_diskio_unregister(char path[4]);

#ifdef __cplusplus
}
#endif

#endif
