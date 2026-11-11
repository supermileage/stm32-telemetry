/*
 * flowmeter_task.c
 *
 *  Created on: Nov 7, 2026
 *      Author: Faysal
 */

#include "ina228.h"
#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

void FlowMeter_task(void *parameters) {
	//device specific parameters

	//TODO: check max current based on shunt
	ina228_t ina228 = {
			.i2c_bus = &i2c2_bus,
			.i2c_errors = 0,
			.shunt_ohms = 0.005f,
			.max_current_a = 30.0f,
			.adc_range_high = false
	};

	INA228_init(&ina228);

	for (;;) {
		//block ourselves until interrupt or 100ms pass
		//use pdTRUE to act as a binary semaphore as drain clears everything no need to build up multiple drains
		ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));

		float current_a = INA228_read_current(&ina228);
		if(current_a < -0.2){ //negative current means error
			printf("Error with reading current\r\n");
			//restart sensor/handle error
		}
		printf("Current is %fA\r\n", current_a);

		float voltage_shunt_v = INA228_read_voltage_shunt(&ina228);
		printf("Voltage is %fV\r\n", voltage_shunt_v);
	}
}
