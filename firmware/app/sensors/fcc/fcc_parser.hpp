#pragma once

#include "fcc_record.h"
#include "fcc_protocol.hpp"

#include <cstddef>
#include <cstdint>

enum class FccParseResult {
    None,
    Record,
    Invalid
};

/** Decode one complete packet. Invalid input leaves out unchanged. */
bool fcc_decode_packet(const std::uint8_t *bytes, std::size_t length,
                       fcc_measurement_t &out);

class FccParser {
public:
    static constexpr std::size_t PacketSize = fcc_protocol::PacketSize;

    /** gap_ms is the minimum silence separating packets; must be positive. */
    explicit constexpr FccParser(std::uint32_t gap_ms)
        : gap_ms_(gap_ms) {}

    FccParser(const FccParser &) = delete;
    FccParser &operator=(const FccParser &) = delete;

    /**
     * Add a byte; a gap completes the previous packet and starts the next.
     * Only Record updates out. arrival_ms is the byte's receive time.
     */
    FccParseResult feed(
        std::uint8_t byte,
        std::uint32_t arrival_ms,
        fcc_measurement_t &out);

    /**
     * Finish a packet after enough silence. Call only when the RX queue is empty.
     * Only Record updates out.
     */
    FccParseResult poll(
        std::uint32_t now_ms,
        fcc_measurement_t &out);

    /** Reset synchronization; discard the next burst to find a packet boundary. */
    void reset();

private:
    FccParseResult finish(fcc_measurement_t &out);

    // Minimum idle time used to distinguish separate packets.
    const std::uint32_t gap_ms_;

    // Buffer holding the bytes of the current packet.
    std::uint8_t packet_[PacketSize]{};

    // Number of bytes received in the current burst.
    std::size_t count_{};

    // Arrival time of the most recently received byte.
    std::uint32_t last_byte_ms_{};

    // Whether at least one byte has been received.
    bool have_byte_{};

    // Whether packet boundaries have been established.
    bool synchronized_{};

    // Whether more than PacketSize bytes arrived in the current burst.
    bool overflow_{};
};
