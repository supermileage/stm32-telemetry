/**
 * @file
 * @brief Storage task startup and debugger-visible bring-up results.
 */
#ifndef STORAGE_TASK_H
#define STORAGE_TASK_H

#include "storage.h"

/** Startup progress only; later logging failures are reported by logger_get_status(). */
typedef enum {
    /** No storage task has begun running. */
    STORAGE_NOT_STARTED,
    /** Configuring the card and mounting the volume. */
    STORAGE_MOUNTING,
    /** Task creation or mounting failed. */
    STORAGE_MOUNT_FAILED,
    /** Diagnostic mode: writing and reading back the text test file. */
    STORAGE_VERIFYING,
    /** Diagnostic mode: the file check failed. */
    STORAGE_VERIFY_FAILED,
    /** Card mounted; logger setup may still be pending. */
    STORAGE_READY,
    /** One-shot test passed and the card was unmounted. */
    STORAGE_TEST_PASSED,
    /** One-shot file check passed, but unmounting failed. */
    STORAGE_TEST_FAILED
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
 * @brief Create the storage task and logger queue before the scheduler starts.
 * Mounts the card and opens the log without running the file self-test.
 * Check startup errors in logger status or the debugger. The first start call
 * chooses the boot mode; reboot to restart after shutdown or failure.
 */
void storage_start_task(void);

/**
 * @brief Run the one-shot file self-test instead of logging.
 * Call before the scheduler and producers start. Creates no logger queue.
 * Wait for STORAGE_TEST_PASSED after verification and unmounting before removing
 * the card. The first start call chooses the boot mode; reboot to run again.
 */
void storage_start_hello_test(void);

#endif
