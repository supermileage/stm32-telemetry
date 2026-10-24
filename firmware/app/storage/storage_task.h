/**
 * @file
 * @brief Storage task startup and debugger-visible bring-up results.
 */
#ifndef STORAGE_TASK_H
#define STORAGE_TASK_H

#include "storage.h"

/** Startup progress only; later logging failures are reported by logger_get_status(). */
typedef enum {
    STORAGE_NOT_STARTED,   /**< No storage task has begun running. */
    STORAGE_MOUNTING,      /**< Configuring the card and mounting the volume. */
    STORAGE_MOUNT_FAILED,  /**< Task creation or mounting failed. */
    STORAGE_VERIFYING,     /**< Diagnostic mode: writing and reading back the text test file. */
    STORAGE_VERIFY_FAILED, /**< Diagnostic mode: the file check failed. */
    STORAGE_READY,         /**< Card mounted; logger setup may still be pending. */
    STORAGE_TEST_PASSED,   /**< One-shot test passed and the card was unmounted. */
    STORAGE_TEST_FAILED    /**< One-shot file check passed, but unmounting failed. */
} storage_bringup_state_t;

/** Startup progress for the debugger; not a task synchronization mechanism. */
extern volatile storage_bringup_state_t storage_bringup_state;
/** Mount result, initially FR_NOT_READY; FR_INT_ERR can indicate task creation failure. */
extern volatile FRESULT storage_mount_result;
/** Diagnostic verification result; remains FR_NOT_READY in normal mode. */
extern volatile FRESULT storage_verify_result;
/** Unmount result; FR_NOT_READY until cleanup runs. */
extern volatile FRESULT storage_unmount_result;
/** Diagnostic self-test path; empty in normal mode. */
extern char storage_test_path[STORAGE_TEST_PATH_SIZE];

/**
 * Creates the storage task and logger queue before the scheduler starts.
 *
 * Once running, the task mounts the SD card and opens the binary log.
 * The file self-test runs only with storage_start_hello_test(). Startup
 * errors can be checked in the debugger or through logger_get_status().
 *
 * The startup mode is set once per boot when the task is created.
 * Later calls have no effect. Restarting after shutdown or failure
 * requires a reboot.
 */
void storage_start_task(void);

/**
 * Starts a one-time Hello World test instead of the telemetry logger.
 *
 * Call before the scheduler and producers start. The task writes a text
 * file to the SD card, reads it back to verify the contents, and unmounts
 * the card. No logger queue is created.
 *
 * Wait for STORAGE_TEST_PASSED before removing the card or powering off.
 *
 * Only one startup mode can be selected per boot. The first successful
 * task creation determines the mode. Mount and verification errors
 * are reported separately.
 */
void storage_start_hello_test(void);

#endif
