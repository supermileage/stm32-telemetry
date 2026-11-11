#include "gnss_parser.hpp"
#include "minmea.h"

static bool rmc_calendar(const minmea_sentence_rmc &rmc, sample_calendar_t &out)
{
    /** RMC years 80 through 99 mean 1980 through 1999, outside the RTC's range. */
    if (!rmc.valid || rmc.date.year < 0 || rmc.date.year > 79 ||
        rmc.time.microseconds < 0 || rmc.time.microseconds > 999999) {
        return false;
    }

    const sample_calendar_t utc{
        static_cast<uint16_t>(2000 + rmc.date.year),
        static_cast<uint8_t>(rmc.date.month), static_cast<uint8_t>(rmc.date.day),
        static_cast<uint8_t>(rmc.time.hours), static_cast<uint8_t>(rmc.time.minutes),
        static_cast<uint8_t>(rmc.time.seconds),
        static_cast<uint16_t>(rmc.time.microseconds / 1000)
    };
    if (!sample_calendar_is_valid(&utc)) {
        return false;
    }

    out = utc;
    return true;
}

gnss_parse_result_t GnssParser::decode_sentence(gnss_sample_t &out)
{
    /** Require a matching checksum before decoding any fields. */
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
    out.utc_valid = rmc_calendar(rmc, out.utc);
    out.measurement.fix_valid = rmc.valid && rmc.latitude.scale > 0 && rmc.longitude.scale > 0;
    out.measurement.latitude = {rmc.latitude.value, rmc.latitude.scale};
    out.measurement.longitude = {rmc.longitude.value, rmc.longitude.scale};
    out.measurement.speed = {rmc.speed.value, rmc.speed.scale};

    return GNSS_PARSE_RECORD;
}

void GnssParser::reset()
{
    length_ = 0U;
}

gnss_parse_result_t GnssParser::feed(uint8_t byte, gnss_sample_t &out)
{
    if (byte == '$') {
        length_ = 0U;
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
