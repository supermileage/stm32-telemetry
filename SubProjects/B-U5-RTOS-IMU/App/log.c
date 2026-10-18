/*
 * log.c
 *
 *  Created on: Oct 17, 2026
 *      Author: Faysal
 */

#include "log.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <stdio.h>

extern UART_HandleTypeDef huart1; //temp

void log_write(imu_data_t *data){
	char buffer[128];
	char *name;
	switch (data->type){
		case GYROSCOPE:
			name = "Gyroscope";
			break;
		case ACCELEROMETER:
			name = "Accelerometer";
			break;
		case TIMESTAMP:
			name = "Timestamp";
			break;
		default:
			name = "Unknown";
			break;
	}
	int len = snprintf(buffer, sizeof(buffer), "IMU tick: %ld, timestamp: %f, timeslot: %d \n %s : x:%+04ld, y:%+04ld, z:%+04ld\r\n", data->ticks, data->time, data->timeslot, name, data->data.x, data->data.y, data->data.z);
	if (len >= (int)sizeof(buffer)) len = sizeof(buffer) - 1;
	HAL_UART_Transmit(&huart1, (uint8_t*)buffer, len, 200);
}
