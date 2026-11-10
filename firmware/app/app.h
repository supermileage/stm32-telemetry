// Start the application from the generated RTOS setup.
#ifndef TELEMETRY_APP_H
#define TELEMETRY_APP_H

#ifdef __cplusplus
extern "C" {
#endif

// Create the logger queue and the logger and sensor tasks.
// Call after peripheral and kernel setup, before starting the scheduler.
void app_init(void);

#ifdef __cplusplus
}
#endif

#endif
