// Build GPS sentences from incoming UART bytes.
#ifndef GNSS_PARSER_HPP
#define GNSS_PARSER_HPP

#include "gnss_parser.h"

#include <cstddef>
#include <cstdint>

class GnssParser {
public:
    static constexpr std::size_t LineCapacity = 128U;

    constexpr GnssParser() = default;
    GnssParser(const GnssParser &) = delete;
    GnssParser &operator=(const GnssParser &) = delete;

    // Add one received byte. GNSS_PARSE_RECORD means out holds a new sample.
    gnss_parse_result_t feed(uint8_t byte, uint32_t tick_ms, log_record_t &out);

    // Discard the partial sentence and keep the counters.
    void reset();

    gnss_parser_stats_t stats() const { return stats_; }

private:
    gnss_parse_result_t decode_sentence(log_record_t &out);

    char line_[LineCapacity]{};
    std::size_t length_{};
    sample_timestamp_t acquired_{};
    gnss_parser_stats_t stats_{};
};

#endif
