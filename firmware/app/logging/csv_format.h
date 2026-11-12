/** Convert saved measurements to CSV text without accessing storage. */
#ifndef CSV_FORMAT_H
#define CSV_FORMAT_H

#include "record.h"

#include <stddef.h>

#define CSV_TIMESTAMP_HEADER "elapsed_ms,utc,time_status"
#define GNSS_CSV_HEADER CSV_TIMESTAMP_HEADER ",fix_valid,latitude_deg,longitude_deg,speed_m_s"
#define IMU_CSV_HEADER CSV_TIMESTAMP_HEADER ", IMU ticks, IMU time, data type, timeslot, x, y, z"
#define BMS_STATUS_HEADER CSV_TIMESTAMP_HEADER ", discharge enabled, charge enabled, fault id"
#define BMS_PACK_HEADER CSV_TIMESTAMP_HEADER ", state of charge (soc), voltage, current"
#define BMS_CELL_HEADER CSV_TIMESTAMP_HEADER ", voltage min, voltage max, voltage avg"
#define BMS_TEMPERATURE_HEADER CSV_TIMESTAMP_HEADER ", temperature min, temperature max, temperature avg, bms temperature"
#define CSV_ROW_CAPACITY 256U

/**
 * Write elapsed time, UTC, and clock status without a trailing comma or newline.
 * Returns the character count, or -1 for invalid time or insufficient space.
 */
int csv_format_timestamp(const sample_timestamp_t *timestamp, char *out, size_t capacity);

/**
 * Write one GPS record as a row without a line ending.
 * Returns the character count, or -1 for invalid input or insufficient space.
 */
int gnss_csv_format(const log_record_t *record, char *out, size_t capacity);
int imu_csv_format(const log_record_t *record, char *out, size_t capacity);
int bms_status_csv_format(const log_record_t *record, char *out, size_t capacity);
int bms_pack_csv_format(const log_record_t *record, char *out, size_t capacity);
int bms_cell_csv_format(const log_record_t *record, char *out, size_t capacity);
int bms_temperature_csv_format(const log_record_t *record, char *out, size_t capacity);

#endif
