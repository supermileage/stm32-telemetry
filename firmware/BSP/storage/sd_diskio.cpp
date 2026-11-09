#include "sd_diskio.h"
#include "sd_spi_port.hpp"
#include "ff_gen_drv.h"
#include "stm32u5xx_hal.h"

namespace {
bool registered;

DRESULT finish(SD_Card *card, int result)
{
    // A protocol or transport failure requires card initialization before reuse.
    if (result != SD_OK && result != SD_PARAM_ERR) {
        SD_DeInit_Card(card);
    }
    if (!sd_spi_port_release()) {
        return RES_ERROR;
    }
    if (result == SD_OK) {
        return RES_OK;
    }
    return result == SD_PARAM_ERR ? RES_PARERR : RES_ERROR;
}

DSTATUS initialize(BYTE drive)
{
    if (drive != 0U) {
        return STA_NOINIT;
    }
    SD_Card *card = sd_spi_port_acquire();
    if (card == nullptr) {
        return STA_NOINIT;
    }
    HAL_Delay(10U);
    // Successful initialization returns a positive card type.
    int result = SD_Init_Card(card);
    return finish(card, result > 0 ? SD_OK : result) == RES_OK ? 0U : STA_NOINIT;
}

DSTATUS status(BYTE drive)
{
    return drive == 0U && sd_spi_port_is_ready() ? 0U : STA_NOINIT;
}

DRESULT read(BYTE drive, BYTE *buffer, DWORD sector, UINT count)
{
    if (drive != 0U || buffer == nullptr || count == 0U || count > UINT32_MAX / SD_BLOCK_SIZE) {
        return RES_PARERR;
    }
    if (!sd_spi_port_is_ready()) {
        return RES_NOTRDY;
    }
    SD_Card *card = sd_spi_port_acquire();
    if (card == nullptr) {
        return RES_ERROR;
    }
    return finish(card, SD_Read_Multi_Block_Card(card, sector, buffer, count));
}

DRESULT write(BYTE drive, const BYTE *buffer, DWORD sector, UINT count)
{
    if (drive != 0U || buffer == nullptr || count == 0U || count > UINT32_MAX / SD_BLOCK_SIZE) {
        return RES_PARERR;
    }
    if (!sd_spi_port_is_ready()) {
        return RES_NOTRDY;
    }
    SD_Card *card = sd_spi_port_acquire();
    if (card == nullptr) {
        return RES_ERROR;
    }
    return finish(card, SD_Write_Multi_Block_Card(card, sector, buffer, count));
}

DRESULT ioctl(BYTE drive, BYTE command, void *buffer)
{
    if (drive != 0U || (command != CTRL_SYNC && buffer == nullptr) ||
        (command != CTRL_SYNC && command != GET_SECTOR_COUNT &&
         command != GET_SECTOR_SIZE && command != GET_BLOCK_SIZE)) {
        return RES_PARERR;
    }
    if (!sd_spi_port_is_ready()) {
        return RES_NOTRDY;
    }
    SD_Card *card = sd_spi_port_acquire();
    if (card == nullptr) {
        return RES_ERROR;
    }
    int result = SD_OK;
    switch (command) {
    case CTRL_SYNC: {
        // Writes wait for programming. Also check the card's error status.
        uint16_t card_status = 0U;
        result = SD_Get_Status_Card(card, &card_status);
        if (result == SD_OK && card_status != 0U) {
            result = SD_ERR;
        }
        break;
    }
    case GET_SECTOR_COUNT:
        *static_cast<DWORD *>(buffer) = SD_Get_Info_Card(card)->block_count;
        break;
    case GET_SECTOR_SIZE:
        *static_cast<WORD *>(buffer) = SD_BLOCK_SIZE;
        break;
    case GET_BLOCK_SIZE:
        // Use one sector as the erase-unit hint. Card geometry is not exposed.
        *static_cast<DWORD *>(buffer) = 1U;
        break;
    }
    return finish(card, result);
}

const Diskio_drvTypeDef driver = {initialize, status, read, write, ioctl};
} // namespace

bool sd_diskio_register(char path[4])
{
    if (path == nullptr || registered || FATFS_GetAttachedDriversNbr() != 0U ||
        FATFS_LinkDriver(&driver, path) != 0U) {
        return false;
    }
    registered = true;
    return true;
}

bool sd_diskio_unregister(char path[4])
{
    if (path == nullptr || !registered || path[0] != '0' || path[1] != ':' ||
        FATFS_UnLinkDriver(path) != 0U) {
        return false;
    }
    registered = false;
    path[0] = '\0';
    return true;
}
