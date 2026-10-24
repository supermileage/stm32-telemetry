/**
 * @file
 * @brief Application source IDs identifying sensor payload formats.
 */
#ifndef SENSOR_SOURCES_H
#define SENSOR_SOURCES_H

/** Source IDs used by sensor records. Keep them stable for PC decoders.
 * Allocate a new ID if a payload format changes incompatibly. */
typedef enum {
    /** Source ID reserved for IMU samples. */
    LOG_SOURCE_IMU = 1
} sensor_source_t;

#endif
