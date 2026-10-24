/**
 * @file
 * @brief Sample format shared by telemetry producers and the logger queue.
 */
#ifndef LOG_RECORD_H
#define LOG_RECORD_H

#include <stdint.h>

/** Maximum number of payload bytes in one record. */
#define LOG_PAYLOAD_MAX 64U

/**
 * One telemetry sample, copied when submitted to the logger.
 * Initialize the whole record. Producers choose source IDs and encode their
 * payload fields explicitly, including byte order; this struct is not a disk format.
 */
typedef struct {
    /** Milliseconds since boot; wraps to zero after UINT32_MAX. */
    uint32_t timestamp_ms;
    /** Application-assigned ID identifying the payload format. */
    uint16_t source;
    /** Number of payload bytes to write, from 0 to LOG_PAYLOAD_MAX. */
    uint16_t length;
    /** Encoded sample; bytes after length are not written. */
    uint8_t payload[LOG_PAYLOAD_MAX];
} log_record_t;

#endif
