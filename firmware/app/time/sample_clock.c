#include "sample_clock.h"
#include "stm32u5xx_hal.h"

#include <stddef.h>

/* All shared state is protected by saving and restoring the interrupt mask. */
static volatile uint32_t last_tick_ms;
static volatile uint64_t elapsed_ms;
static volatile uint64_t anchor_elapsed_ms;
static volatile int64_t anchor_utc_ms;
static volatile bool synchronized;

static uint32_t lock_clock(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    return mask;
}

static void unlock_clock(uint32_t mask)
{
    __set_PRIMASK(mask);
}

/* Call with interrupts masked so the tick and the extension stay together. */
static uint64_t update_elapsed(void)
{
    uint32_t tick_ms = HAL_GetTick();
    elapsed_ms += (uint32_t)(tick_ms - last_tick_ms);
    last_tick_ms = tick_ms;
    return elapsed_ms;
}

void sample_clock_tick(void)
{
    /* The regular HAL tick prevents a whole wrap from passing between reads. */
    (void)sample_clock_elapsed_ms();
}

uint64_t sample_clock_elapsed_ms(void)
{
    uint32_t mask = lock_clock();
    uint64_t now = update_elapsed();
    unlock_clock(mask);
    return now;
}

sample_timestamp_t sample_clock_now(void)
{
    sample_timestamp_t timestamp = {0};
    (void)sample_clock_from_tick(HAL_GetTick(), &timestamp);
    return timestamp;
}

bool sample_clock_from_tick(uint32_t tick_ms, sample_timestamp_t *out)
{
    if (out == NULL) {
        return false;
    }

    sample_timestamp_t timestamp = {0};
    uint32_t mask = lock_clock();
    uint64_t now = update_elapsed();
    uint32_t age_ms = (uint32_t)now - tick_ms;
    bool has_utc = synchronized;
    uint64_t gps_elapsed_ms = anchor_elapsed_ms;
    int64_t gps_utc_ms = anchor_utc_ms;
    unlock_clock(mask);

    if (age_ms > now) {
        return false;
    }
    timestamp.elapsed_ms = now - age_ms;

    if (has_utc && timestamp.elapsed_ms >= gps_elapsed_ms) {
        uint64_t gps_age_ms = timestamp.elapsed_ms - gps_elapsed_ms;
        if (gps_age_ms <= (uint64_t)(INT64_MAX - gps_utc_ms)) {
            timestamp.utc_ms = gps_utc_ms + (int64_t)gps_age_ms;
            timestamp.status = gps_age_ms < SAMPLE_CLOCK_GPS_TIMEOUT_MS
                ? SAMPLE_TIME_GPS : SAMPLE_TIME_HOLDOVER;
        }
    }

    *out = timestamp;
    return true;
}

bool sample_clock_sync_utc(uint64_t acquired_ms, int64_t utc_ms)
{
    uint32_t mask = lock_clock();
    uint64_t now = update_elapsed();
    bool accepted = acquired_ms <= now && utc_ms > 0;

    /* A repeated or delayed message must not refresh an old GPS time. */
    if (synchronized && (acquired_ms <= anchor_elapsed_ms || utc_ms <= anchor_utc_ms)) {
        accepted = false;
    }

    if (accepted) {
        anchor_elapsed_ms = acquired_ms;
        anchor_utc_ms = utc_ms;
        synchronized = true;
    }

    unlock_clock(mask);
    return accepted;
}
