/**
 * @file
 * @brief Application source IDs identifying sensor payload formats.
 */
#ifndef SENSOR_SOURCES_H
#define SENSOR_SOURCES_H

/* Keep IDs stable for PC decoders. Allocate a new ID for an incompatible format. */
typedef enum {
    LOG_SOURCE_IMU = 1
} sensor_source_t;

#endif
