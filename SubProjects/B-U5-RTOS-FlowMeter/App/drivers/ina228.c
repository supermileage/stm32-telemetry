/*
 * ina228.c
 *
 *	Driver code by Krish, adapted to use in this project
 *
 *  Created on: Nov 7, 2026
 *      Author: Faysal
 */

#include "ina228.h"
#include "util.h"

#include <stdint.h>

static int32_t sign_extend_20_to_32(uint32_t val) {
    // 32 - 20 = 12 bits of shifting required
    return ((int32_t)(val << 12)) >> 12;
}


void INA228_compute_calibration(ina228_t *ina228) {
	// CURRENT_LSB = Imax / 2^19  (matches your earlier logic)
	ina228->current_lsb = ina228->max_current_a / (float) (1UL << 19);

	// SHUNT_CAL = 13107.2e6 * CURRENT_LSB * Rshunt
	// (this constant matches the formula you were using)
	const float k = 1.31072e10f;
	float cal = k * ina228->current_lsb * ina228->shunt_ohms;

	// If ADCRANGE = 1 (high range), multiply by 4 (same as your earlier logic)
	if (ina228->adc_range_high)
		cal *= 4.0f;

	if (cal < 0.0f)
		cal = 0.0f;
	if (cal > 65535.0f)
		cal = 65535.0f;

	uint16_t shunt_cal = (uint16_t) (cal + 0.5f);
	shunt_cal &= 0x7FFF; //mask to 14 bits
	ina228->shunt_cal = shunt_cal;
}

void INA228_setup(ina228_t *ina228) {
	uint16_t adc_config = 0xFB68;
	if (ina228->adc_range_high)
		adc_config |= (1u << 4);
	else
		adc_config &= ~(1u << 4);


	uint8_t config[] = {0x00, 0x00}; //+-163.84mV range
	HAL_TRY(i2c_write(ina228->i2c_bus, INA228_ADDRESS, INA228_REG_CONFIG, config, 2));
	uint8_t adc_config_[] = {(uint8_t)(adc_config >> 8), (uint8_t)(adc_config & 0xFF)};
	HAL_TRY(i2c_write(ina228->i2c_bus, INA228_ADDRESS, INA228_REG_ADC_CONFIG, adc_config_, 2));
	uint8_t shunt_cal[] = {(uint8_t)(ina228->shunt_cal >> 8), (uint8_t)(ina228->shunt_cal & 0xFF)};
	HAL_TRY(i2c_write(ina228->i2c_bus, INA228_ADDRESS, INA228_REG_SHUNT_CAL, shunt_cal, 2));
}

void INA228_init(ina228_t *ina228){
	INA228_compute_calibration(ina228);
	INA228_setup(ina228);
}

//reads current in amps
//negative value = error
//TODO: better way to signal error
float INA228_read_current(ina228_t *ina228) {
	uint8_t current_raw[3];
	if(i2c_read(ina228->i2c_bus, INA228_ADDRESS, INA228_REG_CURRENT, current_raw, 3) != HAL_OK) { return -1.0f; }
	uint32_t current_combined = combine_u8_to_u32(0x00, current_raw[0], current_raw[1], current_raw[2]);
	// CURRENT is 20-bit two's complement in bits [23:4]
	current_combined >>= 4;

	int32_t current = sign_extend_20_to_32(current_combined);

	return (float) current * ina228->current_lsb;
}

float INA228_read_voltage_shunt(ina228_t *ina228) {
	uint8_t voltage_raw[3];
	if(i2c_read(ina228->i2c_bus, INA228_ADDRESS, 0x04, voltage_raw, 3) != HAL_OK) { return -1.0f; }
	uint32_t voltage_combined = combine_u8_to_u32(0x00, voltage_raw[0], voltage_raw[1], voltage_raw[2]);
	// CURRENT is 20-bit two's complement in bits [23:4]
	voltage_combined >>= 4;

	int32_t current = sign_extend_20_to_32(voltage_combined);

	return (float) current * 312.5e-9f;
}


