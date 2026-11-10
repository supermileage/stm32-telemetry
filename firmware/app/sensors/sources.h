// Application source IDs identifying sensor payload formats.
#ifndef SENSOR_SOURCES_H
#define SENSOR_SOURCES_H

// Source IDs reserved for sensor records. Keep them stable for PC decoders.
// Allocate a new ID if a payload format changes incompatibly.
typedef enum {
    LOG_SOURCE_IMU = 1,
    LOG_SOURCE_GPS = 2
} sensor_source_t;

#endif
