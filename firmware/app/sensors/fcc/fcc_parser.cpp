#include "fcc_parser.hpp"

bool fcc_decode_packet(const std::uint8_t *bytes, std::size_t length,
                       fcc_measurement_t &out)
{
    if (bytes == nullptr || length != fcc_protocol::PacketSize) {
        return false;
    }

    // Current is the only two-byte field, sent most significant byte first.
    const std::uint16_t current = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[fcc_protocol::FuelCellCurrentHighIndex]) << 8U) |
        bytes[fcc_protocol::FuelCellCurrentLowIndex]);

    out.error_flag = bytes[fcc_protocol::ErrorFlagIndex];
    out.ambient_temperature_mc = bytes[fcc_protocol::AmbientTemperatureIndex] *
        fcc_protocol::AmbientTemperatureMcPerCount;
    out.fuel_cell_voltage_mv = bytes[fcc_protocol::FuelCellVoltageIndex] *
        fcc_protocol::FuelCellVoltageMvPerCount;
    out.hydrogen_leak_voltage_mv = bytes[fcc_protocol::HydrogenLeakVoltageIndex] *
        fcc_protocol::HydrogenLeakVoltageMvPerCount;
    out.fuel_cell_temperature_mc = bytes[fcc_protocol::FuelCellTemperatureIndex] *
        fcc_protocol::FuelCellTemperatureMcPerCount;
    out.fuel_cell_current_ma = current * fcc_protocol::FuelCellCurrentMaPerCount;
    out.battery_voltage_mv = bytes[fcc_protocol::BatteryVoltageIndex] *
        fcc_protocol::BatteryVoltageMvPerCount;
    return true;
}

FccParseResult FccParser::finish(fcc_measurement_t &out)
{
    FccParseResult result = FccParseResult::None;

    // Discard the first burst to establish packet synchronization.
    if (synchronized_) {

        // Reject oversized bursts, including an otherwise valid packet prefix.
        if (!overflow_ &&
            fcc_decode_packet(packet_, count_, out)) {

            result = FccParseResult::Record;
        } else {
            result = FccParseResult::Invalid;
        }
    }

    // Prepare the parser for the next burst.
    count_ = 0U;
    overflow_ = false;
    synchronized_ = true;

    return result;
}


FccParseResult FccParser::feed(
    std::uint8_t byte,
    std::uint32_t arrival_ms,
    fcc_measurement_t &out)
{
    if (gap_ms_ == 0U) {
        return FccParseResult::Invalid;
    }

    FccParseResult result = FccParseResult::None;

    // Check whether enough time has passed since the previous byte.
    // Unsigned subtraction also handles timer wraparound.
    if (have_byte_ &&
        static_cast<std::uint32_t>(arrival_ms - last_byte_ms_) >= gap_ms_) {

        // Finish the previous burst.
        result = finish(out);
    }

    // Store the newly received byte.
    if (count_ < PacketSize) {
        packet_[count_] = byte;
        ++count_;
    } else {
        // Mark the entire burst invalid if it contains extra bytes.
        overflow_ = true;
    }

    // Remember when this byte arrived.
    last_byte_ms_ = arrival_ms;
    have_byte_ = true;

    return result;
}


FccParseResult FccParser::poll(
    std::uint32_t now_ms,
    fcc_measurement_t &out)
{
    if (gap_ms_ == 0U) {
        return FccParseResult::Invalid;
    }

    // Nothing to finish if no bytes have been received.
    if (!have_byte_) {
        return FccParseResult::None;
    }

    // Check whether enough silence has passed since the last byte.
    if (static_cast<std::uint32_t>(now_ms - last_byte_ms_) < gap_ms_) {
        return FccParseResult::None;
    }

    // The current burst has finished. Decode it.
    const FccParseResult result = finish(out);

    // No active burst remains.
    have_byte_ = false;

    return result;
}


void FccParser::reset()
{
    // Discard all partially received data.
    count_ = 0U;
    last_byte_ms_ = 0U;

    have_byte_ = false;
    synchronized_ = false;
    overflow_ = false;
}
