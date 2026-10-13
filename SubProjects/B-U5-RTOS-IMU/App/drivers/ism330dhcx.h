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

typedef struct{
	I2C_HandleTypeDef *i2c;
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

void IMU_init(imu_t *imu);
void IMU_drain_fifo(imu_t *imu);
void IMU_log_clock_pair(imu_t *imu);


#endif /* APP_DRIVERS_ISM330DHCX_H_ */
