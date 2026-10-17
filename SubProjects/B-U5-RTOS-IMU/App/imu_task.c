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
	imu_t imu = {
			.i2c = (I2C_HandleTypeDef *)parameters,
			.i2c_errors = 0
	};

	IMU_init(&imu);
	uint8_t consecutive_empty_reads = 0; //how many reads in a row resulted in no data

	for (;;) {
		//block ourselves until interrupt or 100ms pass
		//use pdTRUE to act as a binary semaphore as drain clears everything no need to build up multiple drains
		ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));

		//if not reading any data multiple times in a row then IMU might've lost power or reset, init it again
		uint16_t words_read = IMU_drain_fifo(&imu);
		if(words_read > 0) { consecutive_empty_reads = 0; }
		else if(++consecutive_empty_reads >= 5){
			uint8_t ODR_XL;
			HAL_StatusTypeDef status = imu_read_registers(&imu, 0x10, &ODR_XL, 1);
			if(status == HAL_OK && ODR_XL != 0b01001000){
				IMU_init(&imu);
			}
			consecutive_empty_reads = 0;
		}

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
