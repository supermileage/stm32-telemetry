/**
 * @file
 * @brief SD volume setup owned by the logger task.
 */
#ifndef STORAGE_H
#define STORAGE_H

#include "ff.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mount the SD card as FatFs drive 0 without formatting.
 * Call after the scheduler starts.
 * @return FR_OK on success, otherwise a setup or FatFs error.
 */
FRESULT storage_mount(void);

/**
 * @brief Unmount the card and unregister its driver.
 * Flush buffers and close files first. Safe to call after a failed mount.
 * @return FR_OK on success, otherwise a cleanup error.
 */
FRESULT storage_unmount(void);

#ifdef __cplusplus
}
#endif

#endif
