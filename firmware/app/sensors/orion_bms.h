/*
 * orion_bms.h
 *
 *  Created on: Nov 11, 2026
 *      Author: Faysal
 */

#ifndef SENSORS_ORION_BMS_H_
#define SENSORS_ORION_BMS_H_

#define BMS_SOC_SCALING_FACTOR 		2.0f
#define BMS_VOLTAGE_SCALING_FACTOR 	10.0f
#define BMS_CURRENT_SCALING_FACTOR 	10.0f
#define BMS_CELL_SCALING_FACTOR 	1000.0f

int parseFault(uint8_t data[]);

#endif /* SENSORS_ORION_BMS_H_ */
