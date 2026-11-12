#pragma once
#include <stdint.h>

// Decoded values: mc = thousandths of a degree Celsius, mv = millivolts, ma = milliamps.
typedef struct {
    uint8_t error_flag;
    int32_t ambient_temperature_mc;
    int32_t fuel_cell_voltage_mv;
    int32_t hydrogen_leak_voltage_mv;
    int32_t fuel_cell_temperature_mc;
    int32_t fuel_cell_current_ma;
    int32_t battery_voltage_mv;
} fcc_measurement_t;
