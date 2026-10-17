/*
 * imu_task.c
 *
 *  Created on: Oct 12, 2026
 *      Author: Faysal
 */

#include "imu_task.h"
#include "ism330dhcx.h"
#include "stm32u5xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* Preserve the local project's UART4 output; the driver has no UART/SD coupling. */
extern UART_HandleTypeDef huart4;

static void print_status(const char *operation, HAL_StatusTypeDef result)
{
	char buffer[80];
	int length = snprintf(buffer, sizeof(buffer), "IMU %s failed: HAL status %d\r\n",
		operation, (int)result);
	if (length > 0) {
		uint16_t size = (uint16_t)((size_t)length < sizeof(buffer) ? (size_t)length : sizeof(buffer) - 1);
		(void)HAL_UART_Transmit(&huart4, (uint8_t *)buffer, size, 200);
	}
}

static void print_xyz(const char *name, const xyz_t *sample)
{
	char buffer[96];
	int length = snprintf(buffer, sizeof(buffer), "%s: x:%+ld, y:%+ld, z:%+ld\r\n",
		name, (long)sample->x, (long)sample->y, (long)sample->z);
	if (length > 0) {
		uint16_t size = (uint16_t)((size_t)length < sizeof(buffer) ? (size_t)length : sizeof(buffer) - 1);
		(void)HAL_UART_Transmit(&huart4, (uint8_t *)buffer, size, 200);
	}
}

static TaskHandle_t IMU_task_handle = NULL; //for the ISR

void IMU_task(void *parameters) {
	IMU_task_handle = xTaskGetCurrentTaskHandle();
	imu_t imu = { .i2c = (I2C_HandleTypeDef *)parameters };

	HAL_StatusTypeDef result;
	while ((result = IMU_init(&imu)) != HAL_OK) {
		print_status("initialization", result);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
	TickType_t last_output = xTaskGetTickCount();
	uint32_t last_gyro_samples = 0;
	uint32_t last_accel_samples = 0;

	for (;;) {
		//block ourselves until interrupt or 100ms pass
		//use pdTRUE to act as a binary semaphore as drain clears everything no need to build up multiple drains
		ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
		result = IMU_drain_fifo(&imu);
		if (result != HAL_OK && result != HAL_BUSY) {
			print_status("FIFO read", result);
			vTaskDelay(pdMS_TO_TICKS(100));
			continue;
		}
		result = IMU_log_clock_pair(&imu);
		if (result != HAL_OK) {
			print_status("clock read", result);
		}
		/* Decimate only UART output; draining every sample must not wait on UART. */
		TickType_t now = xTaskGetTickCount();
		if ((TickType_t)(now - last_output) >= pdMS_TO_TICKS(100)) {
			if (imu.gyroscope_samples != last_gyro_samples) {
				print_xyz("gyroscope (mdps)", &imu.gyroscope);
			}
			if (imu.accelerometer_samples != last_accel_samples) {
				print_xyz("accelerometer (mg)", &imu.accelerometer);
			}
			last_gyro_samples = imu.gyroscope_samples;
			last_accel_samples = imu.accelerometer_samples;
			last_output = now;
		}
	}
}

void IMU_INT1_from_ISR(void) {
	if (IMU_task_handle != NULL) { //check NULL so no segfault
		BaseType_t higher_priority_waken = pdFALSE;
		vTaskNotifyGiveFromISR(IMU_task_handle, &higher_priority_waken);
		portYIELD_FROM_ISR(higher_priority_waken);
	}
}
