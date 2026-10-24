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
 * @brief Mount the card without formatting it. Storage task only.
 * Close files and unmount after an I/O error before retrying.
 * @return FR_OK if mounted, otherwise a setup or FatFs error.
 */
FRESULT storage_mount(void);

/**
 * @brief Unmount and unlink the card driver. Storage task only.
 * Close all files first; no other FatFs or SD calls may be running.
 * @return FR_OK if unmounted, otherwise the cleanup error.
 */
FRESULT storage_unmount(void);

/**
 * @brief Create an unused HELLOxxx.TXT and verify its contents.
 * Storage task only, with the card mounted. Names run from 000 to 999.
 * @param[out] path Non-NULL buffer of STORAGE_TEST_PATH_SIZE bytes; receives the attempted path.
 *                  Empty if no file was attempted.
 * @return FR_OK if verified, FR_EXIST if names are exhausted, FR_DISK_ERR for
 *         a short write or content mismatch, or another FatFs error.
 */
FRESULT storage_self_test(char path[STORAGE_TEST_PATH_SIZE]);

#endif
