/**
 * @file
 * @brief Application startup entry point called by the generated C RTOS code.
 */
#ifndef TELEMETRY_APP_H
#define TELEMETRY_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the storage task and logger queue.
 * Call after peripheral and kernel setup, before starting the scheduler.
 */
void app_init(void);

#ifdef __cplusplus
}
#endif

#endif
