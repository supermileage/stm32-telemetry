/*
 * ism330dhcx.h
 *
 *  Created on: Oct 12, 2026
 *      Author: Faysal
 */

#ifndef APP_DRIVERS_ISM330DHCX_H_
#define APP_DRIVERS_ISM330DHCX_H_

#include <stdint.h>
#include "stm32u5xx_hal.h"

#define IMU_ADDRESS 0b11010110

#define CHUNK 32	//max 256 or else can have high and low byte mismatch
#define BYTES_PER_WORD 7

#define STARTUP_MS 35

#define MAX_I2C_ATTEMPTS 3
#define IMU_I2C_TIMEOUT_MS 50

#define SCL_PIN GPIO_PIN_4
#define SCL_PORT GPIOH
#define SDA_PIN GPIO_PIN_5
#define SDA_PORT GPIOH

typedef struct{
	I2C_HandleTypeDef *i2c;
	unsigned int i2c_errors;
	float mg_per_lsb;
	float mdps_per_lsb;
	double timestamp_resolution;
	double timestamp_offset;
} imu_t;

typedef struct{
	int32_t x, y, z;
} xyz_t;

typedef enum {
	GYROSCOPE = 0x01,
	ACCELEROMETER = 0x02,
	TIMESTAMP = 0x04
} IMU_FIFO_TAG;

typedef enum {
	I2C_READ,
	I2C_WRITE
} i2c_direction_t;

void IMU_init(imu_t *imu);
uint16_t IMU_drain_fifo(imu_t *imu);
void IMU_log_clock_pair(imu_t *imu);

HAL_StatusTypeDef i2c_read(I2C_HandleTypeDef *i2c, uint16_t device_address, uint8_t register_address, uint8_t *data, uint16_t length);


#endif /* APP_DRIVERS_ISM330DHCX_H_ */
