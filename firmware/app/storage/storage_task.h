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
    STORAGE_VERIFYING,     /**< Writing and reading back the text test file. */
    STORAGE_VERIFY_FAILED, /**< The file check failed. */
    STORAGE_READY,         /**< Mount and file check passed; logger setup may still be pending. */
    STORAGE_TEST_PASSED,   /**< One-shot test passed and the card was unmounted. */
    STORAGE_TEST_FAILED    /**< One-shot file check passed, but unmounting failed. */
} storage_bringup_state_t;

/** Startup progress for the debugger; not a task synchronization mechanism. */
extern volatile storage_bringup_state_t storage_bringup_state;
/** Mount result, initially FR_NOT_READY; FR_INT_ERR can indicate task creation failure. */
extern volatile FRESULT storage_mount_result;
/** Text-file verification result; FR_NOT_READY until the check runs. */
extern volatile FRESULT storage_verify_result;
/** Unmount result; FR_NOT_READY until cleanup runs. */
extern volatile FRESULT storage_unmount_result;
/** Last self-test path; inspect after verification has completed. */
extern char storage_test_path[STORAGE_TEST_PATH_SIZE];

/**
 * Create the storage task and logger queue before the scheduler and producers start.
 * The task mounts the card, runs the file check, then opens the binary log.
 * Startup failures appear in the debugger results and logger_get_status().
 * Choose this or storage_start_hello_test() per boot; after task creation,
 * later start calls do nothing. Restart after shutdown or failure requires a reboot.
 */
void storage_start_task(void);

/**
 * Start a one-shot Hello World test instead of the telemetry logger.
 * Call before the scheduler and producers start. The task creates a text file,
 * checks its contents, then unmounts the card. No logger queue is created.
 * Wait for STORAGE_TEST_PASSED before removing the card or cutting power.
 * Choose one start function per boot; the first successful task creation selects
 * the mode. Mount and verification failures retain their specific failure states.
 */
void storage_start_hello_test(void);

#endif
