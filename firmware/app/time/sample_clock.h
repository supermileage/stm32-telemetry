// Shared elapsed time and GPS-based UTC for sensor samples.
#ifndef SAMPLE_CLOCK_H
#define SAMPLE_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mark UTC as holdover after this many milliseconds without a GPS update.
#define SAMPLE_CLOCK_GPS_TIMEOUT_MS 3000U

typedef enum {
    SAMPLE_TIME_UNSYNCED,
    SAMPLE_TIME_GPS,
    SAMPLE_TIME_HOLDOVER
} sample_time_status_t;

typedef struct {
    // Milliseconds since boot, including HAL tick wraps.
    uint64_t elapsed_ms;

    // UTC milliseconds since 1970, or zero until GPS time is available.
    int64_t utc_ms;
    sample_time_status_t status;
} sample_timestamp_t;

// Keep the extended clock current even when sensors are idle.
// Call from the TIM1 callback after HAL_IncTick(). No initialization is needed.
void sample_clock_tick(void);

// Read elapsed time from a task or interrupt. Safe before the scheduler starts.
uint64_t sample_clock_elapsed_ms(void);

// Capture the time when a measurement becomes available, from a task or interrupt.
// GPS updates can correct UTC, but they never change elapsed time.
sample_timestamp_t sample_clock_now(void);

// Recover a buffered byte's timestamp from the HAL tick saved when it arrived.
// Use a past tick less than one 32-bit wrap old, about 49.7 days.
// Returns false for NULL or a time before boot, leaving out unchanged.
// Data received before the current GPS update is marked unsynchronized.
bool sample_clock_from_tick(uint32_t tick_ms, sample_timestamp_t *out);

// Update UTC using when a GPS message arrived and the time it reported.
// Call from the GPS task after checking the message's date and time.
// Both times must advance past the previous update. Future reception times
// and UTC values of zero or less are rejected. Returns true when accepted.
// Receiver and UART delays are not corrected here.
bool sample_clock_sync_utc(uint64_t acquired_ms, int64_t utc_ms);

#ifdef __cplusplus
}
#endif

#endif
