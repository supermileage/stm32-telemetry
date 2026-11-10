#include "gnss_parser.hpp"
#include "sources.h"
#include "minmea.h"

enum { GNSS_PAYLOAD_SIZE = 25U };

static_assert(GNSS_PAYLOAD_SIZE <= LOG_PAYLOAD_MAX, "GNSS payload is too large");

static bool leap_year(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static bool rmc_utc_ms(const struct minmea_sentence_rmc *rmc, int64_t *utc_ms)
{
    const struct minmea_date *date = &rmc->date;
    const struct minmea_time *time = &rmc->time;
    static const int month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (!rmc->valid || date->year < 0 || date->year > 99 ||
        date->month < 1 || date->month > 12 || date->day < 1 ||
        time->hours < 0 || time->hours > 23 || time->minutes < 0 || time->minutes > 59 ||
        time->seconds < 0 || time->seconds > 59 ||
        time->microseconds < 0 || time->microseconds > 999999) {
        return false;
    }

    /* Match minmea's century rule: 00 to 79 are 2000 to 2079. */
    int year = date->year < 80 ? 2000 + date->year : 1900 + date->year;
    int days_in_month = month_days[date->month - 1];
    if (date->month == 2 && leap_year(year)) {
        ++days_in_month;
    }
    if (date->day > days_in_month) {
        return false;
    }

    /* Count days directly so UTC does not depend on timezone or time_t width. */
    int64_t days = 0;
    for (int y = 1970; y < year; ++y) {
        days += leap_year(y) ? 366 : 365;
    }
    for (int month = 1; month < date->month; ++month) {
        days += month_days[month - 1];
        if (month == 2 && leap_year(year)) {
            ++days;
        }
    }
    days += date->day - 1;

    *utc_ms = ((days * 24 + time->hours) * 60 + time->minutes) * 60000
        + time->seconds * 1000 + time->microseconds / 1000;
    return true;
}

static void put_i32_le(uint8_t *out, int32_t value)
{
    uint32_t bits = (uint32_t)value;
    for (unsigned i = 0U; i < 4U; ++i) {
        out[i] = (uint8_t)(bits >> (8U * i));
    }
}

gnss_parse_result_t GnssParser::decode_sentence(log_record_t &out)
{
    /* Strict mode checks the checksum and rejects sentences without one. */
    enum minmea_sentence_id id = minmea_sentence_id(line_, true);
    if (id == MINMEA_INVALID) {
        return GNSS_PARSE_INVALID;
    }
    if (id != MINMEA_SENTENCE_RMC) {
        return GNSS_PARSE_NONE;
    }

    minmea_sentence_rmc rmc{};
    if (!minmea_parse_rmc(&rmc, line_)) {
        return GNSS_PARSE_INVALID;
    }

    out = {};
    out.timestamp = acquired_;
    out.source = LOG_SOURCE_GPS;
    out.length = GNSS_PAYLOAD_SIZE;

    int64_t utc_ms;
    if (rmc_utc_ms(&rmc, &utc_ms) &&
        sample_clock_sync_utc(acquired_.elapsed_ms, utc_ms)) {
        /* This message supplies the UTC anchor for its own acquisition time. */
        out.timestamp.utc_ms = utc_ms;
        out.timestamp.status = SAMPLE_TIME_GPS;
        ++stats_.utc_updates;
    } else {
        ++stats_.rejected_utc_updates;
    }

    /* Keep the existing GPS payload: validity, coordinates, and speed fractions. */
    out.payload[0] = rmc.valid && rmc.latitude.scale > 0 && rmc.longitude.scale > 0;
    const int32_t fields[] = {
        rmc.latitude.value, rmc.latitude.scale,
        rmc.longitude.value, rmc.longitude.scale,
        rmc.speed.value, rmc.speed.scale
    };
    for (size_t i = 0U; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        put_i32_le(&out.payload[1U + 4U * i], fields[i]);
    }

    return GNSS_PARSE_RECORD;
}

void GnssParser::reset()
{
    length_ = 0U;
}

gnss_parse_result_t GnssParser::feed(uint8_t byte, uint32_t tick_ms, log_record_t &out)
{
    if (byte == '$') {
        length_ = 0U;
        if (!sample_clock_from_tick(tick_ms, &acquired_)) {
            return GNSS_PARSE_INVALID;
        }
        line_[0] = '$';
        length_ = 1U;
    } else if (length_ == 0U) {
        return GNSS_PARSE_NONE;
    } else if (byte == '\n') {
        line_[length_] = '\0';
        length_ = 0U;
        return decode_sentence(out);
    } else if (byte == '\r') {
        return GNSS_PARSE_NONE;
    } else if (byte < 0x20U || byte > 0x7eU) {
        length_ = 0U;
        return GNSS_PARSE_INVALID;
    } else if (length_ < sizeof(line_) - 1U) {
        line_[length_++] = (char)byte;
    } else {
        length_ = 0U;
        return GNSS_PARSE_OVERLONG;
    }

    return GNSS_PARSE_NONE;
}

/* The C acquisition task uses one statically allocated parser. */
static GnssParser task_parser;

extern "C" void gnss_parser_reset(void)
{
    task_parser.reset();
}

extern "C" gnss_parse_result_t gnss_parser_feed(uint8_t byte, uint32_t tick_ms,
                                               log_record_t *out)
{
    return out != nullptr ? task_parser.feed(byte, tick_ms, *out) : GNSS_PARSE_INVALID;
}

extern "C" gnss_parser_stats_t gnss_parser_get_stats(void)
{
    return task_parser.stats();
}
