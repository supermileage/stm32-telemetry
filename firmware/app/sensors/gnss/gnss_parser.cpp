#include "gnss_parser.hpp"
#include "minmea.h"

namespace {
constexpr int64_t MicrodegreesPerDegree = 1000000;
constexpr int64_t MillidegreesPerDegree = 1000;
constexpr int64_t MinutesPerDegree = 60;
constexpr int64_t NmeaDegreeFactor = 100;
constexpr int64_t MaxLatitudeDegrees = 90;
constexpr int64_t MaxLongitudeDegrees = 180;
constexpr int64_t FullTurnDegrees = 360;
constexpr int64_t MillimetresPerNauticalMile = 1852000;
constexpr int64_t SecondsPerHour = 3600;

// Convert signed NMEA degrees/minutes without losing precision to floating point.
bool coordinate_microdegrees(const minmea_float &raw, int64_t max_degrees, int32_t &out)
{
    if (raw.scale <= 0) {
        return false;
    }

    const int64_t magnitude = raw.value < 0 ? -static_cast<int64_t>(raw.value) : raw.value;
    const int64_t degrees = magnitude / (NmeaDegreeFactor * raw.scale);
    const int64_t minutes = magnitude % (NmeaDegreeFactor * raw.scale);
    if (minutes >= MinutesPerDegree * raw.scale || degrees > max_degrees ||
        (degrees == max_degrees && minutes != 0)) {
        return false;
    }

    const int64_t value = degrees * MicrodegreesPerDegree +
        minutes * MicrodegreesPerDegree / (MinutesPerDegree * raw.scale);
    out = static_cast<int32_t>(raw.value < 0 ? -value : value);
    return true;
}

bool speed_millimetres_per_second(const minmea_float &raw, int32_t &out)
{
    if (raw.scale <= 0 || raw.value < 0) {
        return false;
    }

    const int64_t value = static_cast<int64_t>(raw.value) * MillimetresPerNauticalMile /
        (SecondsPerHour * raw.scale);
    if (value > INT32_MAX) {
        return false;
    }
    out = static_cast<int32_t>(value);
    return true;
}

bool course_millidegrees(const minmea_float &raw, int32_t &out)
{
    const int64_t full_turn = FullTurnDegrees * raw.scale;
    if (raw.scale <= 0 || raw.value < 0 || raw.value > full_turn) {
        return false;
    }

    // Exactly 360 degrees is north. Truncation keeps all other values below 360.
    out = raw.value == full_turn ? 0 :
        static_cast<int32_t>(static_cast<int64_t>(raw.value) * MillidegreesPerDegree / raw.scale);
    return true;
}

bool rmc_calendar(const minmea_sentence_rmc &rmc, sample_calendar_t &out)
{
    // RMC years 80 through 99 are outside the RTC's range.
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

} // namespace

gnss_parse_result_t GnssParser::decode_sentence(gnss_sample_t &out)
{
    // Require a matching checksum before decoding any fields.
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
    auto &measurement = out.measurement;
    int32_t latitude = 0;
    int32_t longitude = 0;
    measurement.fix_valid = rmc.valid &&
        coordinate_microdegrees(rmc.latitude, MaxLatitudeDegrees, latitude) &&
        coordinate_microdegrees(rmc.longitude, MaxLongitudeDegrees, longitude);
    if (measurement.fix_valid) {
        measurement.latitude_udeg = latitude;
        measurement.longitude_udeg = longitude;
        measurement.speed_valid = speed_millimetres_per_second(rmc.speed, measurement.speed_mm_s);
        measurement.course_valid = course_millidegrees(rmc.course, measurement.course_mdeg);
    }

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
