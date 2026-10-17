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
 * Configure the SD SPI interface after peripheral setup and RTOS kernel init.
 * Call once before the scheduler starts. A setup failure enters Error_Handler().
 * This does not initialize the card or start the storage and logging tasks.
 */
void app_init(void);

#ifdef __cplusplus
}
#endif

#endif
