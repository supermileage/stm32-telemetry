/*
 * i2c.c
 *
 *  Created on: Oct 17, 2026
 *      Author: Faysal
 */

#include "i2c.h"

#include "FreeRTOS.h"
#include "task.h"
#include "log.h"
#include "main.h"


//add one for each used I2C bus
extern I2C_HandleTypeDef hi2c2;
i2c_bus_t i2c2_bus = {
		&hi2c2,
		I2C2_SCL_GPIO_Port,
		I2C2_SCL_Pin,
		I2C2_SDA_GPIO_Port,
		I2C2_SDA_Pin
};

static HAL_StatusTypeDef try_transfer(I2C_HandleTypeDef *i2c_handle, uint16_t device_address, i2c_direction_t direction, uint8_t register_address, uint8_t *data, uint16_t length) {
	HAL_StatusTypeDef status = HAL_ERROR;
	for (int attempt = 0; attempt < MAX_I2C_ATTEMPTS && status != HAL_OK; attempt++)
		if(direction == I2C_READ){
			status = HAL_I2C_Mem_Read(i2c_handle, device_address, register_address, I2C_MEMADD_SIZE_8BIT, data, length, IMU_I2C_TIMEOUT_MS);
		}else{
			status = HAL_I2C_Mem_Write(i2c_handle, device_address, register_address, I2C_MEMADD_SIZE_8BIT, data, length, IMU_I2C_TIMEOUT_MS);
		}
	return status;
}



static HAL_StatusTypeDef transfer_with_recovery(i2c_bus_t *i2c_bus, uint16_t device_address, i2c_direction_t direction, uint8_t register_address, uint8_t *data, uint16_t length) {
	//attempt to read
	if (try_transfer(i2c_bus->handle, device_address, direction, register_address, data, length) == HAL_OK) {
		return HAL_OK;
	}
	//here if multiple attempts fail
	//log_write(); //log i2c error

	//try to reinit I2C bus, keeping settings the same
	HAL_I2C_DeInit(i2c_bus->handle);

	//reconfigure SDA and SCL as GPIO so we can toggle them
	GPIO_InitTypeDef pin_config = {
			.Mode = GPIO_MODE_OUTPUT_OD,
			.Pull = GPIO_NOPULL,
			.Speed = GPIO_SPEED_FREQ_LOW
	};
	pin_config.Pin = i2c_bus->scl_pin;
	HAL_GPIO_Init(i2c_bus->scl_port, &pin_config);
	pin_config.Pin = i2c_bus->sda_pin;
	HAL_GPIO_Init(i2c_bus->sda_port, &pin_config);

	//while deinit also pulse I2C clock 9 times to clear any possible partial/torn communication
	HAL_GPIO_WritePin(i2c_bus->sda_port, i2c_bus->sda_pin, GPIO_PIN_SET); // let go of SDA so we can read who's holding it (open drain)
	for (int clock_pulse = 0; clock_pulse < 9 && HAL_GPIO_ReadPin(i2c_bus->sda_port, i2c_bus->sda_pin) == GPIO_PIN_RESET; clock_pulse++) {
		HAL_GPIO_WritePin(i2c_bus->scl_port, i2c_bus->scl_pin, GPIO_PIN_RESET);
		vTaskDelay(pdMS_TO_TICKS(1));
		HAL_GPIO_WritePin(i2c_bus->scl_port, i2c_bus->scl_pin, GPIO_PIN_SET);
		vTaskDelay(pdMS_TO_TICKS(1));
	}
	HAL_GPIO_WritePin(i2c_bus->sda_port, i2c_bus->sda_pin, GPIO_PIN_RESET); // STOP: SDA rises while SCL is high
	vTaskDelay(pdMS_TO_TICKS(1));
	HAL_GPIO_WritePin(i2c_bus->sda_port, i2c_bus->sda_pin, GPIO_PIN_SET);
	vTaskDelay(pdMS_TO_TICKS(1));

	HAL_I2C_Init(i2c_bus->handle);
	return try_transfer(i2c_bus->handle, device_address, direction, register_address, data, length);
}

HAL_StatusTypeDef i2c_read(i2c_bus_t *i2c_bus, uint16_t device_address, uint8_t register_address, uint8_t *data, uint16_t length){
	return transfer_with_recovery(i2c_bus, device_address, I2C_READ, register_address, data, length);
}

HAL_StatusTypeDef i2c_write(i2c_bus_t *i2c_bus, uint16_t device_address, uint8_t register_address, const uint8_t *data, uint16_t length){
	return transfer_with_recovery(i2c_bus, device_address, I2C_WRITE, register_address, (uint8_t *)data, length);
}
