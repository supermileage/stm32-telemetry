/** Measurements copied from sensor tasks into the logger queue. */
#ifndef LOG_RECORD_H
#define LOG_RECORD_H

#include "time/sample_clock.h"
#include "sensors/gnss_record.h"
#include "sensors/ism330dhcx.h"
#include "sensors/orion_bms.h"

/** Identify which data member is filled. The IMU source is reserved for later. */
typedef enum {
    LOG_SOURCE_IMU,
    LOG_SOURCE_GPS,
	LOG_SOURCE_BMS_STATUS,
	LOG_SOURCE_BMS_PACK,
	LOG_SOURCE_BMS_CELL,
	LOG_SOURCE_BMS_TEMPERATURE
} sensor_source_t;

/**
 * One sensor measurement and its capture time. Zero-initialize the record, then
 * set the source and its matching data member. The queue copies this struct.
 * CSV formatters write individual fields rather than the struct's raw bytes.
 */
typedef struct {
    /** Capture time supplied by the sensor task. */
    sample_timestamp_t timestamp;

    sensor_source_t source;

    union {
        gnss_measurement_t gps;
        imu_data_t imu;
        bms_status_t bms_status;
        bms_pack_t bms_pack;
        bms_cell_t bms_cell;
        bms_temperature_t bms_temperature;
    } data;
} log_record_t;

#ifdef __cplusplus
#include <type_traits>

static_assert(std::is_trivially_copyable_v<log_record_t>,
              "FreeRTOS queues copy records as bytes");
#endif

#endif
