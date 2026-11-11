/*
 * i2c.h
 *
 *  Created on: Oct 17, 2026
 *      Author: Faysal
 */

#ifndef APP_I2C_H_
#define APP_I2C_H_

#include "stm32u5xx_hal.h"
#include <stdint.h>

#define MAX_I2C_ATTEMPTS 3
#define IMU_I2C_TIMEOUT_MS 50 //size to 2-3x longer than max time a read/cycle will take. calculate with how much data to read/write and i2c bus speed

typedef struct {
	I2C_HandleTypeDef *handle;
	GPIO_TypeDef *scl_port;
	uint16_t scl_pin;
	GPIO_TypeDef *sda_port;
	uint16_t sda_pin;
} i2c_bus_t;

typedef enum {
	I2C_READ,
	I2C_WRITE
} i2c_direction_t;

//add one for each used I2C bus
extern i2c_bus_t i2c2_bus;

HAL_StatusTypeDef i2c_read(i2c_bus_t *i2c_bus, uint16_t device_address, uint8_t register_address, uint8_t *data, uint16_t length);
HAL_StatusTypeDef i2c_write(i2c_bus_t *i2c_bus, uint16_t device_address, uint8_t register_address, const uint8_t *data, uint16_t length);

#endif /* APP_I2C_H_ */
