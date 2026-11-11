#include "sample_clock.h"
#include "rtc.h"

#include <cstddef>

/** Save and restore the interrupt mask whenever accessing this shared state. */
static volatile uint32_t last_tick_ms;
static volatile uint64_t elapsed_ms;
static volatile bool rtc_initialized;

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

/** Call with interrupts masked so the HAL tick and 64-bit total stay together. */
static uint64_t update_elapsed(void)
{
    uint32_t tick_ms = HAL_GetTick();
    elapsed_ms += (uint32_t)(tick_ms - last_tick_ms);
    last_tick_ms = tick_ms;

    return elapsed_ms;
}

void sample_clock_tick(void)
{
    /** Updating every HAL tick ensures that no counter wrap is missed. */
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
    sample_timestamp_t timestamp{};
    uint32_t mask = lock_clock();
    timestamp.elapsed_ms = update_elapsed();

    if (rtc_initialized && !__HAL_RTC_SHIFT_GET_FLAG(&hrtc, RTC_FLAG_SHPF)) {
        RTC_TimeTypeDef time{};
        RTC_DateTypeDef date{};
        const auto time_result = HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
        /** Reading the date releases the RTC snapshot, even if the time read failed. */
        const auto date_result = HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

        if (time_result == HAL_OK && date_result == HAL_OK &&
            time.SubSeconds <= time.SecondFraction) {
            const uint32_t ticks_per_second = time.SecondFraction + 1U;
            timestamp.utc = {
                static_cast<uint16_t>(2000U + date.Year), date.Month, date.Date,
                time.Hours, time.Minutes, time.Seconds,
                static_cast<uint16_t>(((time.SecondFraction - time.SubSeconds) * 1000U +
                    ticks_per_second / 2U) / ticks_per_second)
            };
            if (sample_calendar_is_valid(&timestamp.utc)) {
                timestamp.status = SAMPLE_TIME_RTC;
            }
        }
    }

    unlock_clock(mask);

    return timestamp;
}

bool sample_clock_utc_is_set(void)
{
    uint32_t mask = lock_clock();
    bool initialized = rtc_initialized;
    unlock_clock(mask);

    return initialized;
}

static uint8_t rtc_weekday(const sample_calendar_t &utc)
{
    /** HAL needs an initial weekday. The RTC advances it from then on. */
    static constexpr unsigned offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    unsigned year = utc.year - (utc.month < 3U ? 1U : 0U);
    unsigned day = (year + year / 4U - year / 100U + year / 400U +
        offsets[utc.month - 1U] + utc.day) % 7U;

    return day == 0U ? RTC_WEEKDAY_SUNDAY : static_cast<uint8_t>(day);
}

bool sample_clock_init_utc(const sample_calendar_t *utc)
{
    if (__get_IPSR() != 0U || __get_PRIMASK() != 0U ||
        !sample_calendar_is_valid(utc) || sample_clock_utc_is_set()) {
        return false;
    }

    RTC_DateTypeDef date{};
    date.Year = static_cast<uint8_t>(utc->year - 2000U);
    date.Month = utc->month;
    date.Date = utc->day;
    date.WeekDay = rtc_weekday(*utc);

    RTC_TimeTypeDef time{};
    time.Hours = utc->hour;
    time.Minutes = utc->minute;
    time.Seconds = utc->second;
    time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    time.StoreOperation = RTC_STOREOPERATION_RESET;

    /**
     * Leave interrupts enabled while HAL waits for hardware. Readers report
     * UNSYNCED until every step succeeds. Set the time before the date so an old
     * time near midnight cannot roll the new date forward.
     */
    if (HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK ||
        HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK) {
        return false;
    }

    /**
     * SetTime ignores SubSeconds. Apply the GPS fraction with a hardware shift,
     * rounded to the RTC's subsecond resolution.
     */
    const uint32_t ticks_per_second = hrtc.Init.SynchPrediv + 1U;
    const uint32_t fraction = (utc->millisecond * ticks_per_second + 500U) / 1000U;
    if (fraction != 0U && HAL_RTCEx_SetSynchroShift(&hrtc, RTC_SHIFTADD1S_SET,
        ticks_per_second - fraction) != HAL_OK) {
        return false;
    }

    const uint32_t started = HAL_GetTick();
    while (__HAL_RTC_SHIFT_GET_FLAG(&hrtc, RTC_FLAG_SHPF)) {
        if (static_cast<uint32_t>(HAL_GetTick() - started) > RTC_TIMEOUT_VALUE) {
            return false;
        }
    }

    uint32_t mask = lock_clock();
    rtc_initialized = true;
    unlock_clock(mask);

    return true;
}
