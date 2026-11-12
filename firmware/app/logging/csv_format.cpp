#include "csv_format.h"

#include <cstdio>
#include <stdbool.h>

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

static void format_coordinate(int32_t value, int32_t scale, char out[24])
{
    if (scale <= 0) {
        out[0] = '\0';
        return;
    }

    int64_t magnitude = value < 0 ? -(int64_t)value : value;
    int64_t degrees = magnitude / (100LL * scale);
    int64_t minutes = magnitude % (100LL * scale);
    int64_t converted = degrees * 1000000 + minutes * 1000000 / (60LL * scale);

    (void)snprintf(out, 24U, "%s%lu.%06lu", value < 0 ? "-" : "",
                   (unsigned long)(converted / 1000000), (unsigned long)(converted % 1000000));
}

static void format_speed(int32_t value, int32_t scale, char out[24])
{
    if (scale <= 0) {
        out[0] = '\0';
        return;
    }

    /** One knot is exactly 463/900 metres per second. */
    int64_t magnitude = value < 0 ? -(int64_t)value : value;
    int64_t converted = magnitude * 463000 / (900LL * scale);

    (void)snprintf(out, 24U, "%s%lu.%03lu", value < 0 ? "-" : "",
                   (unsigned long)(converted / 1000), (unsigned long)(converted % 1000));
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
    bool fix_valid = measurement.fix_valid;
    char latitude[24] = "";
    char longitude[24] = "";
    char speed[24] = "";

    if (fix_valid) {
        format_coordinate(measurement.latitude.value, measurement.latitude.scale, latitude);
        format_coordinate(measurement.longitude.value, measurement.longitude.scale, longitude);
        format_speed(measurement.speed.value, measurement.speed.scale, speed);
    }

    int length = snprintf(out + prefix, capacity - (size_t)prefix, ",%u,%s,%s,%s", fix_valid ? 1U : 0U, latitude, longitude, speed);
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

    int length = snprintf(out + prefix, capacity - (size_t)prefix, ",%lu, %f, %s, %d, %ld, %ld, %ld", ticks, time, type, timeslot, x, y, z);
    return length >= 0 && (size_t)length < capacity - (size_t)prefix ? prefix + length : -1;
}

int bms_status_csv_format(const log_record_t *record, char *out, size_t capacity){
    if (record == nullptr || record->source != LOG_SOURCE_BMS_STATUS) {
        return -1;
    }

    int prefix = csv_format_timestamp(&record->timestamp, out, capacity);
    if (prefix < 0) {
        return -1;
    }

    const bms_status_t &data = record->data.bms_status;
    bool discharge_enabled = data.discharge_enabled;
    bool charge_enabled = data.charge_enabled;
    uint8_t fault_id = data.fault;


    int length = snprintf(out + prefix, capacity - (size_t)prefix, ",%s,%s,%u", discharge_enabled ? "true" : "false", charge_enabled ? "true" : "false", fault_id);
    return length >= 0 && (size_t)length < capacity - (size_t)prefix ? prefix + length : -1;
}

int bms_pack_csv_format(const log_record_t *record, char *out, size_t capacity){
    if (record == nullptr || record->source != LOG_SOURCE_BMS_PACK) {
        return -1;
    }

    int prefix = csv_format_timestamp(&record->timestamp, out, capacity);
    if (prefix < 0) {
        return -1;
    }

    const bms_pack_t &data = record->data.bms_pack;
    float state_of_charge = data.state_of_charge;
    float voltage = data.voltage;
    float current = data.current;


    int length = snprintf(out + prefix, capacity - (size_t)prefix, ",%f,%f,%f", state_of_charge, voltage, current);
    return length >= 0 && (size_t)length < capacity - (size_t)prefix ? prefix + length : -1;
}

int bms_cell_csv_format(const log_record_t *record, char *out, size_t capacity){
    if (record == nullptr || record->source != LOG_SOURCE_BMS_CELL) {
        return -1;
    }

    int prefix = csv_format_timestamp(&record->timestamp, out, capacity);
    if (prefix < 0) {
        return -1;
    }

    const bms_cell_t &data = record->data.bms_cell;
    float voltage_min = data.voltage_min;
    float voltage_max = data.voltage_max;
    float voltage_avg = data.voltage_avg;


    int length = snprintf(out + prefix, capacity - (size_t)prefix, ",%f,%f,%f", voltage_min, voltage_max, voltage_avg);
    return length >= 0 && (size_t)length < capacity - (size_t)prefix ? prefix + length : -1;
}

int bms_temperature_csv_format(const log_record_t *record, char *out, size_t capacity){
    if (record == nullptr || record->source != LOG_SOURCE_BMS_TEMPERATURE) {
        return -1;
    }

    int prefix = csv_format_timestamp(&record->timestamp, out, capacity);
    if (prefix < 0) {
        return -1;
    }

    const bms_temperature_t &data = record->data.bms_temperature;
	uint8_t temperature_min = data.temperature_min;
	uint8_t temperature_max = data.temperature_max;
	uint8_t temperature_avg = data.temperature_avg;
	uint8_t bms_temperature = data.bms_temperature;


    int length = snprintf(out + prefix, capacity - (size_t)prefix, ",%u,%u,%u,%u", temperature_min, temperature_max, temperature_avg, bms_temperature);
    return length >= 0 && (size_t)length < capacity - (size_t)prefix ? prefix + length : -1;
}
