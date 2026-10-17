/**
 * @file
 * @brief SD volume setup and file verification for the single storage task.
 */
#ifndef STORAGE_H
#define STORAGE_H

#include "ff.h"

/** Capacity of the self-test filename, including its NUL terminator. */
#define STORAGE_TEST_PATH_SIZE 17U

/**
 * Mount the SD card without formatting it. Call from the storage task.
 * After an I/O error, close files and unmount before retrying.
 * @return FR_OK if mounted (including an existing mount), or a setup/FatFs error.
 */
FRESULT storage_mount(void);

/**
 * Unmount the card and unlink its driver. Close all files first.
 * Call only from the storage task, with no other FatFs or SD access in progress.
 * @return FR_OK if unmounted or already unregistered; otherwise the cleanup error.
 */
FRESULT storage_unmount(void);

/**
 * Write "Hello World\r\n" to a new HELLOxxx.TXT and read it back.
 * Requires a mounted card and must run in the storage task. Existing files
 * are kept; names run from HELLO000.TXT to HELLO999.TXT.
 * @param[out] path Non-NULL buffer of STORAGE_TEST_PATH_SIZE bytes. Receives
 * the attempted filename, or an empty string if no file was attempted.
 * @return FR_OK after verification and close, FR_EXIST if all names are used,
 * FR_DISK_ERR for a short write or content mismatch, or another FatFs error.
 */
FRESULT storage_self_test(char path[STORAGE_TEST_PATH_SIZE]);

#endif
