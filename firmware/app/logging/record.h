// Sample format shared by telemetry producers and the logger queue.
#ifndef LOG_RECORD_H
#define LOG_RECORD_H

#include "time/sample_clock.h"
#include <stdint.h>

// Maximum number of payload bytes in one record.
#define LOG_PAYLOAD_MAX 64U

// One telemetry sample, copied when submitted to the logger.
// Initialize the whole record. Producers choose source IDs and encode their
// payload fields explicitly, including byte order; this struct is not a disk format.
typedef struct {
    // Time the measurement was acquired. Keep it with the sample.
    sample_timestamp_t timestamp;

    // Identifies the sensor and its payload format.
    uint16_t source;

    // Number of valid payload bytes, up to LOG_PAYLOAD_MAX.
    uint16_t length;

    // Encoded sensor data. Only the first length bytes are used.
    uint8_t payload[LOG_PAYLOAD_MAX];
} log_record_t;

#endif
