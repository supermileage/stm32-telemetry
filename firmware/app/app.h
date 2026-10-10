#ifndef TELEMETRY_APP_H
#define TELEMETRY_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Call after peripheral and RTOS kernel initialization, before the scheduler. */
void app_init(void);

#ifdef __cplusplus
}
#endif

#endif
