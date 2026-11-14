#ifndef SD_DISKIO_H
#define SD_DISKIO_H

// FatFs drive 0 registration for the SD card.
// Logger task only. Configure SPI before registering.

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Register the SD driver as FatFs drive 0.
// Does not initialize the card or mount the filesystem.
// Writes "0:/" into path on success.
// Returns true if registered.
bool sd_diskio_register(char path[4]);

// Unregister the SD driver after closing files and unmounting.
// Pass the registered drive path. It is cleared on success.
// Returns true if unregistered.
bool sd_diskio_unregister(char path[4]);

#ifdef __cplusplus
}
#endif

#endif
