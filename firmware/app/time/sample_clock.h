/** @file Shared acquisition clock and GPS UTC mapping. */
#ifndef SAMPLE_CLOCK_H
#define SAMPLE_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Mark UTC as holdover after this many milliseconds without a GPS update. */
#define SAMPLE_CLOCK_GPS_TIMEOUT_MS 3000U

typedef enum {
    SAMPLE_TIME_UNSYNCED,
    SAMPLE_TIME_GPS,
    SAMPLE_TIME_HOLDOVER
} sample_time_status_t;

typedef struct {
    uint64_t elapsed_ms;          /**< Milliseconds since boot, extended across tick wrap. */
    int64_t utc_ms;               /**< Unix milliseconds, or zero when unsynchronized. */
    sample_time_status_t status;
} sample_timestamp_t;

/**
 * Keep the extended clock current even when sensors are idle.
 * Call from the TIM1 callback after HAL_IncTick(). No initialization is needed.
 */
void sample_clock_tick(void);

/** Read elapsed time from a task or interrupt. Safe before the scheduler starts. */
uint64_t sample_clock_elapsed_ms(void);

/**
 * Capture elapsed time and its UTC mapping together at acquisition.
 * Call from a task or interrupt. GPS updates never change elapsed time.
 * UTC may step when a new GPS update corrects the mapping.
 */
sample_timestamp_t sample_clock_now(void);

/**
 * Map a GPS reception time to the UTC reported by that message.
 * Call from the GPS task after validating the message and its date and time.
 * Both values must be newer than the previous anchor. Future reception times
 * and nonpositive UTC values are rejected without changing the mapping.
 * Returns true when the update is accepted. UART latency is not corrected here.
 */
bool sample_clock_sync_utc(uint64_t acquired_ms, int64_t utc_ms);

#ifdef __cplusplus
}
#endif

#endif
