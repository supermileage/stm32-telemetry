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
 * Create the storage task and logger queue after peripheral setup and RTOS
 * kernel init. Call once before the scheduler starts. The storage task owns
 * SD setup after scheduling begins and reports failures through logger status.
 */
void app_init(void);

#ifdef __cplusplus
}
#endif

#endif
