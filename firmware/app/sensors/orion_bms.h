/*
 * orion_bms.h
 *
 *  Created on: Nov 11, 2026
 *      Author: Faysal
 */

#ifndef SENSORS_ORION_BMS_H_
#define SENSORS_ORION_BMS_H_

#include <stdbool.h>

#define BMS_SOC_SCALING_FACTOR 		2.0f
#define BMS_VOLTAGE_SCALING_FACTOR 	10.0f
#define BMS_CURRENT_SCALING_FACTOR 	10.0f
#define BMS_CELL_SCALING_FACTOR 	1000.0f

typedef struct{
	bool discharge_enabled;
	bool charge_enabled;
	uint8_t fault;
} bms_status_t;

typedef struct{
	float state_of_charge;
	float voltage;
	float current;
} bms_pack_t;

typedef struct{
	float voltage_min;
	float voltage_max;
	float voltage_avg;
} bms_cell_t;

typedef struct{
	uint8_t temperature_min;
	uint8_t temperature_max;
	uint8_t temperature_avg;
	uint8_t bms_temperature;
} bms_temperature_t;


int parseFault(uint8_t data[]);

#endif /* SENSORS_ORION_BMS_H_ */
