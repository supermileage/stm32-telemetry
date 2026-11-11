/** Capture elapsed time and the RTC calendar alongside each sensor measurement. */
#ifndef SAMPLE_CLOCK_H
#define SAMPLE_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Whether this sample has a usable RTC calendar. */
typedef enum {
    SAMPLE_TIME_UNSYNCED,
    SAMPLE_TIME_RTC
} sample_time_status_t;

/** UTC calendar fields. The RTC supports years 2000 through 2099. */
typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t millisecond;
} sample_calendar_t;

/**
 * Check that the calendar contains a real date and time supported by the RTC.
 * Within 2000 through 2099, every year divisible by four is a leap year.
 * Hardware handles calendar advancement after initialization.
 */
static inline bool sample_calendar_is_valid(const sample_calendar_t *utc)
{
    if (!utc || utc->year < 2000U || utc->year > 2099U ||
        utc->month < 1U || utc->month > 12U || utc->day < 1U ||
        utc->hour > 23U || utc->minute > 59U || utc->second > 59U ||
        utc->millisecond > 999U) {
        return false;
    }

    uint8_t days = 31U;
    switch (utc->month) {
    case 2U:
        days = utc->year % 4U == 0U ? 29U : 28U;
        break;

    case 4U:
    case 6U:
    case 9U:
    case 11U:
        days = 30U;
        break;

    default:
        break;
    }

    return utc->day <= days;
}

/** Time captured with a measurement and kept with it until it is logged. */
typedef struct {
    /** Milliseconds since boot, including HAL tick wraps. */
    uint64_t elapsed_ms;

    /** UTC at capture time. Only use this when status is SAMPLE_TIME_RTC. */
    sample_calendar_t utc;

    sample_time_status_t status;
} sample_timestamp_t;

/**
 * Keep elapsed time current while sensors are idle.
 * Call from the TIM1 callback after HAL_IncTick(). No setup is needed.
 */
void sample_clock_tick(void);

/** Read milliseconds since boot from a task or interrupt, even before scheduling. */
uint64_t sample_clock_elapsed_ms(void);

/**
 * Capture the time when a measurement becomes available, from a task or interrupt.
 * Interrupts are briefly masked so the elapsed time and RTC reads stay together.
 */
sample_timestamp_t sample_clock_now(void);

/** Return whether GPS has successfully initialized the RTC during this boot. */
bool sample_clock_utc_is_set(void);

/**
 * Initialize the RTC once from GPS. Call only from the GNSS task after
 * MX_RTC_Init(), with interrupts enabled because HAL setters can wait.
 * Returns false for invalid input, a hardware error, or an already set clock.
 * Failed attempts can be retried. Later GPS messages leave the RTC running.
 * Receiver and UART delays are included. PPS alignment is not implemented.
 */
bool sample_clock_init_utc(const sample_calendar_t *utc);

#ifdef __cplusplus
}
#endif

#endif
