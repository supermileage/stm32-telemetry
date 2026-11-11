/*
 * ina228.h
 *
 *  Created on: Nov 7, 2026
 *      Author: Faysal
 */

#ifndef DRIVERS_INA228_H_
#define DRIVERS_INA228_H_

#include <stdint.h>
#include <stdbool.h>
#include "i2c.h"

#define INA228_ADDRESS 0x40 << 1

// INA228 register map (8-bit register addresses)
#define INA228_REG_CONFIG       	0x00
#define INA228_REG_ADC_CONFIG   	0x01
#define INA228_REG_SHUNT_CAL    	0x02
#define INA228_REG_VOLTAGE_SHUNT	0x04
#define INA228_REG_VOTLAGE_BUS		0x05
#define INA228_REG_CURRENT      	0x07


typedef struct{
	//i2c general
	i2c_bus_t *i2c_bus;
	unsigned int i2c_errors;

	//device specific
    float    shunt_ohms;      // shunt resistance (ohms)
    float    max_current_a;   // expected max current (A)
    bool     adc_range_high;  // ADCRANGE setting
    // computed calibration
    float    current_lsb;     // A/LSB
    uint16_t shunt_cal;       // SHUNT_CAL register
} ina228_t;


void INA228_init(ina228_t *ina228);

//reads current in amps
//negative value = error
float INA228_read_current(ina228_t *ina228);
float INA228_read_voltage_shunt(ina228_t *ina228);
float INA228_read_voltage_bus(ina228_t *ina228);

#endif /* DRIVERS_INA228_H_ */
