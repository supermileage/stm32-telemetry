/**
 * @file sd_diskio.c
 * @brief Optional SD SPI adapter for the STM32Cube FatFs disk dispatcher.
 */
#include "sd_diskio.h"
#include "sd_spi.h"
#include "ff_gen_drv.h"

#include <stddef.h>

static bool registered;

static DRESULT disk_result(sd_result_t result)
{
    switch (result) {
    case SD_OK:
        return RES_OK;
    case SD_NOT_READY:
        return RES_NOTRDY;
    case SD_INVALID_ARGUMENT:
        return RES_PARERR;
    case SD_ERROR:
    default:
        return RES_ERROR;
    }
}

static DSTATUS sd_initialize(BYTE drive)
{
    return drive == 0U && sd_spi_initialize() == SD_OK ? 0U : STA_NOINIT;
}

static DSTATUS sd_status(BYTE drive)
{
    return drive == 0U && sd_spi_is_ready() ? 0U : STA_NOINIT;
}

static DRESULT sd_read(BYTE drive, BYTE *buffer, DWORD sector, UINT count)
{
    return drive == 0U ? disk_result(sd_spi_read(sector, buffer, count)) : RES_PARERR;
}

static DRESULT sd_write(BYTE drive, const BYTE *buffer, DWORD sector, UINT count)
{
    return drive == 0U ? disk_result(sd_spi_write(sector, buffer, count)) : RES_PARERR;
}

static DRESULT sd_ioctl(BYTE drive, BYTE command, void *buffer)
{
    if (drive != 0U || (command != CTRL_SYNC && buffer == NULL)) {
        return RES_PARERR;
    }
    if (command != CTRL_SYNC && command != GET_SECTOR_COUNT &&
        command != GET_SECTOR_SIZE && command != GET_BLOCK_SIZE) {
        return RES_PARERR;
    }
    if (!sd_spi_is_ready()) {
        return RES_NOTRDY;
    }
    switch (command) {
    case CTRL_SYNC:
        return disk_result(sd_spi_sync());
    case GET_SECTOR_COUNT: {
        uint32_t count;
        sd_result_t result = sd_spi_get_sector_count(&count);
        if (result == SD_OK) {
            *(DWORD *)buffer = count;
        }
        return disk_result(result);
    }
    case GET_SECTOR_SIZE:
        *(WORD *)buffer = SD_SPI_SECTOR_SIZE;
        return RES_OK;
    case GET_BLOCK_SIZE:
        /* Formatting is disabled; use one sector for allocation alignment. */
        *(DWORD *)buffer = 1U;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}

static const Diskio_drvTypeDef sd_driver = {
    .disk_initialize = sd_initialize,
    .disk_status = sd_status,
    .disk_read = sd_read,
    .disk_write = sd_write,
    .disk_ioctl = sd_ioctl
};

bool sd_diskio_register(char path[4])
{
    if (path == NULL || registered || FATFS_GetAttachedDriversNbr() != 0U) {
        return false;
    }
    if (FATFS_LinkDriver(&sd_driver, path) != 0U) {
        return false;
    }
    registered = true;
    return true;
}

bool sd_diskio_unregister(char path[4])
{
    if (path == NULL || !registered || path[0] != '0' || path[1] != ':' ||
        FATFS_UnLinkDriver(path) != 0U) {
        return false;
    }
    registered = false;
    path[0] = '\0';
    return true;
}
