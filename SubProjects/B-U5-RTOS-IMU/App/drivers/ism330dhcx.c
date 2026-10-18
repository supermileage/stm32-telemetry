/*
 * ism330dhcx.c
 *
 *  Created on: Oct 12, 2026
 *      Author: Faysal
 */

#include "ism330dhcx.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "log.h"
#include "main.h"
#include "i2c.h"

//return early if HAL error, only use on functions with return type void
#define HAL_TRY(expr) do{ \
		HAL_StatusTypeDef status = (expr); \
		if (status != HAL_OK) return; \
	} while(0)

//TODO: put in a file for util functions
static inline uint16_t min_u16(uint16_t a, uint16_t b) {
	return (a < b) ? a : b;
}

static inline uint16_t combine_u8_to_u16(uint8_t a, uint8_t b) {
	return ((uint16_t) a << 8) | b;
}

static inline uint32_t combine_u8_to_u32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
	return ((uint32_t) a << 24) | ((uint32_t) b << 16) | ((uint32_t) c << 8) | d;
}
//use int32_t for gyroscope, 16 bits maxes out at 57 degrees per second
static void decode_xyz(const uint8_t *data, double scale, xyz_t *out) {
	out->x = (int16_t) combine_u8_to_u16(data[1], data[0]) * scale;
	out->y = (int16_t) combine_u8_to_u16(data[3], data[2]) * scale;
	out->z = (int16_t) combine_u8_to_u16(data[5], data[4]) * scale;
}

void IMU_init(imu_t *imu) {
	TickType_t boot_time = 0;
	vTaskDelayUntil(&boot_time, pdMS_TO_TICKS(STARTUP_MS)); //allow time for IMU to start

	//init
	uint8_t TIMESTAMP_EN = 0b00100000; //start logging time at startup
	HAL_TRY(i2c_write(imu->i2c_bus, IMU_ADDRESS, 0x19, &TIMESTAMP_EN, 1));
	uint8_t DEVICE_CONF = 0b11100010; //set DEVICE_CONF as it's recommended, rest default
	HAL_TRY(i2c_write(imu->i2c_bus, IMU_ADDRESS, 0x18, &DEVICE_CONF, 1));
	uint8_t BDU = 0b01000100; //set BDU = 1, rest default. So high and low bytes don't tear/stay in sync
	HAL_TRY(i2c_write(imu->i2c_bus, IMU_ADDRESS, 0x12, &BDU, 1));
	uint8_t ODR_XL = 0b01001000; //104 Hz sampling rate, 4g FS, default LPF2, last bit must be 0
	HAL_TRY(i2c_write(imu->i2c_bus, IMU_ADDRESS, 0x10, &ODR_XL, 1));
	imu->mg_per_lsb = 0.122f;
	uint8_t ODR_G = 0b01000100; //104 Hz sampling rate, FS: max 500 degrees per second
	HAL_TRY(i2c_write(imu->i2c_bus, IMU_ADDRESS, 0x11, &ODR_G, 1));
	imu->mdps_per_lsb = 17.50f;
	//maybe send reset signal at CTRL3_C

	//FIFO
	uint8_t FIFO_WATERMARK[] = { 0b01111111, 0b00000000 }; //set threshold to alert us to 127 words (6 byte readings + byte for tag)
	HAL_TRY(i2c_write(imu->i2c_bus, IMU_ADDRESS, 0x07, FIFO_WATERMARK, 2));
	uint8_t FIFO_INT1_THRESHOLD = 0b00111000; //throw interrupt when amount of unread words >= watermark (threshold), when about to be full, when overwriting
	HAL_TRY(i2c_write(imu->i2c_bus, IMU_ADDRESS, 0x0D, &FIFO_INT1_THRESHOLD, 1));
	uint8_t FIFO_BDR = 0b01000100; //how fast readings are read into FIFO, good standard of 104hz. BDR<=ODR
	HAL_TRY(i2c_write(imu->i2c_bus, IMU_ADDRESS, 0x09, &FIFO_BDR, 1));
	uint8_t FIFO_mode_time = 0b11000110; //write one timestamp word every 32 slots (slot = 1 tick of BDR), no temperature readings, overwrite old data mode when full
	HAL_TRY(i2c_write(imu->i2c_bus, IMU_ADDRESS, 0x0A, &FIFO_mode_time, 1));
	uint8_t FREQ_FINE_u; //internal IMU clock compensation
	HAL_TRY(i2c_read(imu->i2c_bus, IMU_ADDRESS, 0x63, &FREQ_FINE_u, 1));
	int8_t FREQ_FINE = (int8_t) FREQ_FINE_u;
	double timestamp_resolution = 0.000025 / (1 + 0.0015 * FREQ_FINE); //base of 25us resolution, off by 0.15% per FREQ_FINE
	imu->timestamp_resolution = timestamp_resolution;
	uint8_t timestamp_[4];
	HAL_TRY(i2c_read(imu->i2c_bus, IMU_ADDRESS, 0x40, timestamp_, 4));
	uint32_t start_timestamp = combine_u8_to_u32(timestamp_[3], timestamp_[2], timestamp_[1], timestamp_[0]);
	//how much ahead if the MCU time, to add for future timestamps so they allgn with our time (and other data which will align to your time)
	//using tickcount is a little coarse due to resolution of 1ms, but we don't need to be that precise
	double timestamp_offset = (double) xTaskGetTickCount() / configTICK_RATE_HZ - start_timestamp * timestamp_resolution;
	imu->timestamp_offset = timestamp_offset;

}

uint16_t IMU_drain_fifo(imu_t *imu) {
	//FIFO reading
	uint16_t words_to_read;
	uint16_t words_read = 0;
	uint8_t FIFO_data[CHUNK * BYTES_PER_WORD]; //7 bytes per word
	//read in chunks so that FIFO_data doesn't have to be sized to the worst case scenario of FIFO being completely full
	//also makes it so the I2C bus isn't being hogged for too long
	//reread amount of words left after each chunk to ensure we've emptied it and interrupt is deasserted so we can get a new edge next time
	//rereading also makes sure there is no problem if the high and low byte of DIFF_FIFO are mismatched
	for (;;) {
		uint8_t FIFO_status[2];
		if(i2c_read(imu->i2c_bus, IMU_ADDRESS, 0x3A, FIFO_status, 2) != HAL_OK) { return 0; }

		uint8_t FIFO_full = FIFO_status[1] & 0b00100000; //FIFO will be full in one read
		if (FIFO_full) {
			//log_write();
		}
		uint8_t FIFO_overrun = FIFO_status[1] & 0b00001000; //FIFO has begun to overwrite old data as it's full
		if (FIFO_overrun) {
			//log_write();
		}

		words_to_read = (uint16_t) (((FIFO_status[1] & 0b00000011) << 8) | FIFO_status[0]); //ignore first 6 bits
		if (words_to_read == 0) {
			break;
		} //exit condition
		uint16_t current_chunk = min_u16(words_to_read, CHUNK); //don't read more than we have
		//auto wraps around so we can just burst read
		if(i2c_read(imu->i2c_bus, IMU_ADDRESS, 0x78, FIFO_data, current_chunk * BYTES_PER_WORD) != HAL_OK) { return 0; }
		words_read += current_chunk;

		//process data (tag, timestamp, turn high+low of each measurement to one number)
		for (uint16_t i = 0; i < current_chunk * BYTES_PER_WORD; i += BYTES_PER_WORD) {
			imu_data_t data = {0, 0.0, {0, 0, 0}, 0, 0};
			IMU_FIFO_TAG tag = FIFO_data[i] >> 3; //tag = 5 MSBs
			uint8_t timeslot = FIFO_data[i] >> 1 & 0b00000011;
			switch (tag) {
				case GYROSCOPE: {
					//xyz_t readings;
					decode_xyz(&FIFO_data[i + 1], imu->mdps_per_lsb, &data.data);
					break;
				}
				case ACCELEROMETER: {
					//xyz_t readings;
					decode_xyz(&FIFO_data[i + 1], imu->mg_per_lsb, &data.data);
					break;
				}
				case TIMESTAMP: {
					double fifo_timestamp = combine_u8_to_u32(FIFO_data[i + 4], FIFO_data[i + 3], FIFO_data[i + 2], FIFO_data[i + 1]) * imu->timestamp_resolution + imu->timestamp_offset;
					data.time = fifo_timestamp;
					//other bytes not relevant, ignore them
					break;
				}
				default:
					//error
					break;

			}
			data.type = tag;
			data.timeslot = timeslot;
			log_write(&data);

		}

		//log data (tag, timeslot, data): xQueueSend
	}

	return words_read;
}

void IMU_log_clock_pair(imu_t *imu) {
	//timestamp tick and mcu tick after read for future data processing and compensate for IMU clock errors to have everything in sync
	uint8_t timestamp_[4];
	HAL_TRY(i2c_read(imu->i2c_bus, IMU_ADDRESS, 0x40, timestamp_, 4));
	TickType_t current_tick = xTaskGetTickCount(); //attempt to make tick and timestamp reading as close to the same time as possible
	uint32_t live_timestamp = combine_u8_to_u32(timestamp_[3], timestamp_[2], timestamp_[1], timestamp_[0]);
	//log_write();
}

//void IMU_direct_read(imu_t *imu){
//	//read IMU 0x22-0x2D
//	uint8_t readings[12];
//	HAL_I2C_Mem_Read(imu->i2c, IMU_ADDRESS, 0x22, I2C_MEMADD_SIZE_8BIT, readings, 12, 200);
//
//	//parse data
//	int32_t gyroscope[3];
//	gyroscope[0] = (int16_t)((readings[1] << 8) | readings[0]) * MDPS_PER_LSB;
//	gyroscope[1] = (int16_t)((readings[3] << 8) | readings[2]) * MDPS_PER_LSB;
//	gyroscope[2] = (int16_t)((readings[5] << 8) | readings[4]) * MDPS_PER_LSB;
//	char buffer[64];
//	// Format the signed 16-bit integer into a string
//	int len = snprintf(buffer, sizeof(buffer), "gyroscope   (mdps): x:%+04d, y:%+04d, z:%+04d\r\n", gyroscope[0], gyroscope[1], gyroscope[2]);
//	if (len >= (int)sizeof(buffer)) len = sizeof(buffer) - 1;
//	HAL_UART_Transmit(&huart1, (uint8_t*)buffer, len, 200);
//
//	int16_t accelerometer[3];
//	accelerometer[0] = (int16_t)((readings[7] << 8) | readings[6]) * MG_PER_LSB;
//	accelerometer[1] = (int16_t)((readings[9] << 8) | readings[8]) * MG_PER_LSB;
//	accelerometer[2] = (int16_t)((readings[11] << 8) | readings[10]) * MG_PER_LSB;
//
//	len = snprintf(buffer, sizeof(buffer), "accelerometer (mg): x:%+04d, y:%+04d, z:%+04d\r\n", accelerometer[0], accelerometer[1], accelerometer[2]);
//	if (len >= (int)sizeof(buffer)) len = sizeof(buffer) - 1;
//	HAL_UART_Transmit(&huart1, (uint8_t*)buffer, len, 200);
//}
