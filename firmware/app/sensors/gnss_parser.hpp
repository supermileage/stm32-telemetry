/** Assemble GPS sentences from UART bytes and decode their measurements. */
#ifndef GNSS_PARSER_HPP
#define GNSS_PARSER_HPP

#include "gnss_record.h"
#include "time/sample_clock.h"

#include <cstddef>
#include <cstdint>

/** Result of feeding one byte into the parser. */
typedef enum {
    GNSS_PARSE_NONE,
    GNSS_PARSE_RECORD,
    GNSS_PARSE_INVALID,
    GNSS_PARSE_OVERLONG
} gnss_parse_result_t;

/** Decoded measurement and optional UTC. The GNSS task decides how to use them. */
typedef struct {
    gnss_measurement_t measurement;
    bool utc_valid;
    sample_calendar_t utc;
} gnss_sample_t;

/** Own one sentence buffer without calling the clock, RTOS, or logger. */
class GnssParser {
public:
    static constexpr std::size_t LineCapacity = 128U;

    constexpr GnssParser() = default;
    GnssParser(const GnssParser &) = delete;
    GnssParser &operator=(const GnssParser &) = delete;

    /**
     * Feed one received byte. A complete RMC sentence returns GNSS_PARSE_RECORD
     * and fills out. Every other result leaves out unchanged.
     */
    gnss_parse_result_t feed(uint8_t byte, gnss_sample_t &out);

    /** Discard a partial sentence and wait for the next '$'. */
    void reset();

private:
    gnss_parse_result_t decode_sentence(gnss_sample_t &out);

    char line_[LineCapacity]{};
    std::size_t length_{};
};

#endif
