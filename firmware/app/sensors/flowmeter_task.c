/*
 * flowmeter_task.c
 *
 *  Created on: Nov 7, 2026
 *      Author: Faysal
 */

#include "flowmeter_task.h"
#include "ina228.h"
#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include "logging/logger.h"

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
		float voltage_bus_v = INA228_read_voltage_bus(&ina228);
		if(current_a < -9000.0 || voltage_bus_v < -9000.0){
			printf("Error with reading flow sensor\r\n");
			//restart sensor/handle error
		}else{
			printf("Current is %fA\r\n", current_a);
			printf("Voltage (bus) is %fV\r\n", voltage_bus_v);
			flowmeter_data_t flowmeter_data;
			flowmeter_data.current_a = current_a;
			flowmeter_data.voltage_bus_v = voltage_bus_v;

			log_record_t record;
			record.timestamp = sample_clock_now();
			record.source = LOG_SOURCE_FLOWMETER;
			record.data.flowmeter = flowmeter_data;
			bool accepted = logger_submit(&record);
		}
	}
}

void FlowMeter_start_task(){
	xTaskCreate(FlowMeter_task, "Flow meter init + drain + log", 512, NULL, 2, NULL);
}
