/**
 * @file
 * @brief SD volume lifecycle and a file write/read-back check.
 */
#include "storage.h"
#include "sd_diskio.h"
#include "board_sd.h"

#include <stdio.h>
#include <string.h>

static FATFS filesystem;
static FIL test_file;
static char drive_path[4];
static bool mounted;
static bool registered;

FRESULT storage_mount(void)
{
    if (mounted) {
        return FR_OK;
    }
    if (!board_sd_configure()) {
        return FR_NOT_READY;
    }
    if (!registered) {
        if (!sd_diskio_register(drive_path)) {
            return FR_NOT_ENABLED;
        }
        registered = true;
    }
    FRESULT result = f_mount(&filesystem, drive_path, 1U);
    if (result == FR_OK) {
        mounted = true;
    } else {
        /* Unlinking also clears ST's cached disk initialization flag. */
        (void)f_mount(NULL, drive_path, 0U);
        (void)sd_diskio_unregister(drive_path);
        registered = false;
    }
    return result;
}

FRESULT storage_unmount(void)
{
    if (!registered) {
        return FR_OK;
    }
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

FRESULT storage_self_test(char path[STORAGE_TEST_PATH_SIZE])
{
    static const char message[] = "Hello World\r\n";
    char received[sizeof(message)] = {0};
    if (path == NULL) {
        return FR_INVALID_PARAMETER;
    }
    path[0] = '\0';
    if (!mounted) {
        return FR_NOT_READY;
    }
    FRESULT result = FR_EXIST;
    for (unsigned int number = 0U; number < 1000U; ++number) {
        (void)snprintf(path, STORAGE_TEST_PATH_SIZE, "%sHELLO%03u.TXT", drive_path, number);
        result = f_open(&test_file, path, FA_WRITE | FA_CREATE_NEW);
        if (result != FR_EXIST) {
            break;
        }
    }
    if (result != FR_OK) {
        return result;
    }
    UINT transferred = 0U;
    result = f_write(&test_file, message, sizeof(message) - 1U, &transferred);
    if (result == FR_OK && transferred != sizeof(message) - 1U) {
        result = FR_DISK_ERR;
    }
    if (result == FR_OK) {
        result = f_sync(&test_file);
    }
    FRESULT closed = f_close(&test_file);
    if (result == FR_OK) {
        result = closed;
    }
    if (result != FR_OK) {
        return result;
    }

    /* Reopen the file so the check goes through FatFs again. */
    result = f_open(&test_file, path, FA_READ);
    if (result != FR_OK) {
        return result;
    }
    result = f_read(&test_file, received, sizeof(received), &transferred);
    if (result == FR_OK && (transferred != sizeof(message) - 1U ||
        memcmp(received, message, sizeof(message) - 1U) != 0)) {
        result = FR_DISK_ERR;
    }
    closed = f_close(&test_file);
    return result == FR_OK ? closed : result;
}
