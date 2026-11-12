#pragma once

#include <cstddef>
#include <cstdint>

// FCC wire format and conversions are documented in README.md.

namespace fcc_protocol
{
    // Packet configuration
    constexpr std::size_t PacketSize = 8U;
    constexpr std::uint32_t PacketPeriodMs = 500U;

    // Byte positions (zero-based)
    constexpr std::size_t ErrorFlagIndex = 0U;
    constexpr std::size_t AmbientTemperatureIndex = 1U;
    constexpr std::size_t FuelCellVoltageIndex = 2U;
    constexpr std::size_t HydrogenLeakVoltageIndex = 3U;
    constexpr std::size_t FuelCellTemperatureIndex = 4U;
    constexpr std::size_t FuelCellCurrentHighIndex = 5U;
    constexpr std::size_t FuelCellCurrentLowIndex = 6U;
    constexpr std::size_t BatteryVoltageIndex = 7U;

    // Multiply raw values by these factors to get the named fixed units.
    constexpr std::int32_t AmbientTemperatureMcPerCount = 500;
    constexpr std::int32_t FuelCellVoltageMvPerCount = 333;
    constexpr std::int32_t HydrogenLeakVoltageMvPerCount = 100;
    constexpr std::int32_t FuelCellTemperatureMcPerCount = 500;
    constexpr std::int32_t FuelCellCurrentMaPerCount = 200;
    constexpr std::int32_t BatteryVoltageMvPerCount = 100;
}
