#include "csv_format.h"

#include <cstdio>

static void format_elapsed(uint64_t value, char out[21])
{
    /** Format this ourselves because nano printf cannot print 64-bit integers. */
    char reversed[20];
    size_t length = 0U;

    do {
        reversed[length++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);

    for (size_t i = 0U; i < length; ++i) {
        out[i] = reversed[length - 1U - i];
    }

    out[length] = '\0';
}

int csv_format_timestamp(const sample_timestamp_t *timestamp, char *out, size_t capacity)
{
    if (timestamp == NULL || out == NULL || capacity == 0U) {
        return -1;
    }

    char elapsed[21];
    char utc[32] = "";
    const char *status = "UNSYNCED";

    format_elapsed(timestamp->elapsed_ms, elapsed);

    if (timestamp->status == SAMPLE_TIME_RTC) {
        const auto &calendar = timestamp->utc;
        if (!sample_calendar_is_valid(&calendar)) {
            return -1;
        }

        int length = std::snprintf(utc, sizeof(utc), "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
            static_cast<unsigned>(calendar.year), static_cast<unsigned>(calendar.month),
            static_cast<unsigned>(calendar.day), static_cast<unsigned>(calendar.hour),
            static_cast<unsigned>(calendar.minute), static_cast<unsigned>(calendar.second),
            static_cast<unsigned>(calendar.millisecond));
        if (length < 0 || static_cast<size_t>(length) >= sizeof(utc)) {
            return -1;
        }

        status = "RTC";
    }

    int length = snprintf(out, capacity, "%s,%s,%s", elapsed, utc, status);
    return length >= 0 && (size_t)length < capacity ? length : -1;
}

int gnss_csv_format(const log_record_t *record, char *out, size_t capacity)
{
    if (record == nullptr || record->source != LOG_SOURCE_GPS) {
        return -1;
    }

    int prefix = csv_format_timestamp(&record->timestamp, out, capacity);
    if (prefix < 0) {
        return -1;
    }

    const gnss_measurement_t &measurement = record->data.gps;
    int length = snprintf(out + prefix, capacity - (size_t)prefix,
        ",%u,%u,%ld,%ld,%ld,%ld,%u",
        measurement.fix_valid ? 1U : 0U, measurement.speed_valid ? 1U : 0U,
        (long)measurement.latitude_udeg, (long)measurement.longitude_udeg,
        (long)measurement.speed_mm_s, (long)measurement.course_mdeg,
        measurement.course_valid ? 1U : 0U);
    return length >= 0 && (size_t)length < capacity - (size_t)prefix ? prefix + length : -1;
}

int imu_csv_format(const log_record_t *record, char *out, size_t capacity){
    if (record == nullptr || record->source != LOG_SOURCE_IMU) {
        return -1;
    }

    int prefix = csv_format_timestamp(&record->timestamp, out, capacity);
    if (prefix < 0) {
        return -1;
    }

    const imu_data_t &data = record->data.imu;
    uint32_t ticks = data.ticks;
    double time = data.time;
    const char *type;
    switch (data.type) {
    	case GYROSCOPE:
    		type = "GYROSCOPE";
    		break;
    	case ACCELEROMETER:
    		type = "ACCELEROMETER";
    		break;
    	case TIMESTAMP:
    		type = "TIMESTAMP";
    		break;
    	default:
    		type = "UNKNOWN type";
    		break;
    }
    uint8_t timeslot = data.timeslot;
    int32_t x = data.data.x;
    int32_t y = data.data.y;
    int32_t z = data.data.z;

    int length = snprintf(out + prefix, capacity - (size_t)prefix, ",%lu, %f, %s, %d, %ld, %ld, %ld", (unsigned long)ticks, time, type, timeslot, (long)x, (long)y, (long)z);
    return length >= 0 && (size_t)length < capacity - (size_t)prefix ? prefix + length : -1;
}

int fcc_csv_format(const log_record_t *record, char *out, size_t capacity)
{
    if (record == nullptr || record->source != LOG_SOURCE_FCC) {
        return -1;
    }
    int prefix = csv_format_timestamp(&record->timestamp, out, capacity);
    if (prefix < 0) {
        return -1;
    }
    const auto &measurement = record->data.fcc;
    int length = snprintf(out + prefix, capacity - (size_t)prefix,
        ",%u,%ld,%ld,%ld,%ld,%ld,%ld", (unsigned)measurement.error_flag,
        (long)measurement.ambient_temperature_mc, (long)measurement.fuel_cell_voltage_mv,
        (long)measurement.hydrogen_leak_voltage_mv, (long)measurement.fuel_cell_temperature_mc,
        (long)measurement.fuel_cell_current_ma, (long)measurement.battery_voltage_mv);
    return length >= 0 && (size_t)length < capacity - (size_t)prefix ? prefix + length : -1;
}
