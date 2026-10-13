/*
 * imu_task.c
 *
 *  Created on: Oct 12, 2026
 *      Author: Faysal
 */

#include "ism330dhcx.h"
#include "stm32u5xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

static TaskHandle_t IMU_task_handle = NULL; //for the ISR

void IMU_task(void *parameters) {
	IMU_task_handle = xTaskGetCurrentTaskHandle();
	imu_t imu = { .i2c = (I2C_HandleTypeDef *)parameters };

	IMU_init(&imu);

	for (;;) {
		//block ourselves until interrupt or 100ms pass
		//use pdTRUE to act as a binary semaphore as drain clears everything no need to build up multiple drains
		ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
		IMU_drain_fifo(&imu);
		IMU_log_clock_pair(&imu);
	}
}

void IMU_INT1_from_ISR(void) {
	if (IMU_task_handle != NULL) { //check NULL so no segfault
		BaseType_t higher_priority_waken = pdFALSE;
		vTaskNotifyGiveFromISR(IMU_task_handle, &higher_priority_waken);
		portYIELD_FROM_ISR(higher_priority_waken);
	}
}
