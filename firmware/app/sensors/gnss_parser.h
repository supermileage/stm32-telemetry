// Build GPS sentences from received bytes and decode RMC messages.
#ifndef GNSS_PARSER_H
#define GNSS_PARSER_H

#include "logging/record.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GNSS_PARSE_NONE,
    GNSS_PARSE_RECORD,
    GNSS_PARSE_INVALID,
    GNSS_PARSE_OVERLONG
} gnss_parse_result_t;

typedef struct {
    uint32_t utc_updates;
    uint32_t rejected_utc_updates;
} gnss_parser_stats_t;

// Discard the partial sentence and keep the counters.
void gnss_parser_reset(void);

// Add a byte with the HAL tick saved when it arrived.
// The GNSS task uses one parser. GNSS_PARSE_RECORD means out holds a new sample.
// A null out pointer returns GNSS_PARSE_INVALID. This call does not block.
gnss_parse_result_t gnss_parser_feed(uint8_t byte, uint32_t tick_ms, log_record_t *out);

// Read the parser's counters from the GNSS task.
gnss_parser_stats_t gnss_parser_get_stats(void);

#ifdef __cplusplus
}
#endif

#endif
