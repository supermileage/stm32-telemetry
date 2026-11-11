#include "storage.h"
#include "sd_diskio.h"
#include "sd_card.h"

#include <cstddef>

/* FatFs keeps this object until unmount, so it must outlive storage_mount(). */
static FATFS filesystem;
static char drive_path[4];
static bool mounted;

/* Keep the link until cleanup succeeds, even if mounting failed. */
static bool registered;

FRESULT storage_mount(void)
{
    if (mounted) {
        return FR_OK;
    }

    if (registered) {
        /* Finish a previous cleanup before configuring the card again. */
        return FR_NOT_READY;
    }

    if (!sd_card_configure()) {
        return FR_NOT_READY;
    }

    if (!sd_diskio_register(drive_path)) {
        return FR_NOT_ENABLED;
    }

    registered = true;

    FRESULT result = f_mount(&filesystem, drive_path, 1U);
    if (result == FR_OK) {
        mounted = true;
    } else {
        /* Preserve the mount error. The logger also calls unmount during cleanup. */
        (void)storage_unmount();
    }

    return result;
}

FRESULT storage_unmount(void)
{
    if (!registered) {
        return FR_OK;
    }

    /* Detach FatFs. The logger must close its file first to save data. */
    FRESULT result = f_mount(NULL, drive_path, 0U);
    if (result != FR_OK) {
        return result;
    }

    mounted = false;
    if (!sd_diskio_unregister(drive_path)) {
        return FR_INT_ERR;
    }

    registered = false;
    return FR_OK;
}
