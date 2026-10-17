/*
 * ism330dhcx.c
 *
 *  Created on: Oct 12, 2026
 *      Author: Faysal
 */

#include "ism330dhcx.h"
#include "FreeRTOS.h"
#include "task.h"

#define IMU_IO_TIMEOUT_MS 200U
#define IMU_FIFO_CAPACITY 512U

static uint32_t decode_u32(const uint8_t *data)
{
	return (uint32_t)data[0] | ((uint32_t)data[1] << 8)
		| ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void decode_xyz(const uint8_t *data, double scale, xyz_t *out)
{
	/* Keep gyro values in 32 bits: 500 dps is 500000 mdps. */
	out->x = (int32_t)((int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8)) * scale);
	out->y = (int32_t)((int16_t)((uint16_t)data[2] | ((uint16_t)data[3] << 8)) * scale);
	out->z = (int32_t)((int16_t)((uint16_t)data[4] | ((uint16_t)data[5] << 8)) * scale);
}

static HAL_StatusTypeDef read_registers(imu_t *imu, uint16_t reg,
	uint8_t *data, uint16_t size)
{
	return HAL_I2C_Mem_Read(imu->i2c, IMU_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT,
		data, size, IMU_IO_TIMEOUT_MS);
}

HAL_StatusTypeDef IMU_init(imu_t *imu)
{
	if (imu == NULL || imu->i2c == NULL) {
		return HAL_ERROR;
	}
	*imu = (imu_t){ .i2c = imu->i2c };
	vTaskDelay(pdMS_TO_TICKS(STARTUP_MS));

	uint8_t device_id;
	HAL_StatusTypeDef result = read_registers(imu, 0x0F, &device_id, 1);
	if (result != HAL_OK) {
		return result;
	}
	if (device_id != 0x6B) {
		return HAL_ERROR;
	}

	/* BDU and IF_INC first; bypass clears old FIFO data on a retry. */
	const uint8_t settings[][2] = {
		{ 0x12, 0x44 }, /* CTRL3_C: BDU, address increment */
		{ 0x0A, 0x00 }, /* FIFO bypass */
		{ 0x19, 0x20 }, /* CTRL10_C: timestamp enable */
		{ 0x18, 0xE2 }, /* CTRL9_XL: recommended DEVICE_CONF */
		{ 0x10, 0x48 }, /* CTRL1_XL: 104 Hz, +/-4 g */
		{ 0x11, 0x44 }, /* CTRL2_G: 104 Hz, +/-500 dps */
		{ 0x07, 0x7F }, /* FIFO watermark: 127 words */
		{ 0x08, 0x00 },
		{ 0x0D, 0x38 }, /* INT1 watermark, full, overrun */
		{ 0x09, 0x44 }  /* FIFO gyro and accel batching: 104 Hz */
	};
	for (unsigned int i = 0; i < sizeof(settings) / sizeof(settings[0]); ++i) {
		uint8_t value = settings[i][1];
		result = HAL_I2C_Mem_Write(imu->i2c, IMU_ADDRESS, settings[i][0],
			I2C_MEMADD_SIZE_8BIT, &value, 1, IMU_IO_TIMEOUT_MS);
		if (result != HAL_OK) {
			return result;
		}
	}
	imu->mg_per_lsb = 0.122f;
	imu->mdps_per_lsb = 17.50f;

	uint8_t freq_fine;
	result = read_registers(imu, 0x63, &freq_fine, 1);
	if (result != HAL_OK) {
		return result;
	}
	imu->timestamp_resolution = 0.000025 / (1.0 + 0.0015 * (int8_t)freq_fine);
	uint8_t timestamp[4];
	result = read_registers(imu, 0x40, timestamp, sizeof(timestamp));
	if (result != HAL_OK) {
		return result;
	}
	imu->live_timestamp = decode_u32(timestamp);
	imu->mcu_tick = xTaskGetTickCount();
	imu->timestamp_offset = (double)imu->mcu_tick / configTICK_RATE_HZ
		- imu->live_timestamp * imu->timestamp_resolution;

	/* Continuous FIFO, one timestamp per 32 slots, no temperature batching. */
	uint8_t fifo_mode_time = 0xC6;
	result = HAL_I2C_Mem_Write(imu->i2c, IMU_ADDRESS, 0x0A,
		I2C_MEMADD_SIZE_8BIT, &fifo_mode_time, 1, IMU_IO_TIMEOUT_MS);
	imu->initialized = (result == HAL_OK);
	return result;
}

HAL_StatusTypeDef IMU_drain_fifo(imu_t *imu)
{
	if (imu == NULL || imu->i2c == NULL || !imu->initialized) {
		return HAL_ERROR;
	}
	uint8_t fifo_data[CHUNK * BYTES_PER_WORD];
	uint16_t drained = 0;
	/* Recheck the level after each chunk, but yield if a producer never stops. */
	while (drained < IMU_FIFO_CAPACITY) {
		uint8_t status[2];
		HAL_StatusTypeDef result = read_registers(imu, 0x3A, status, sizeof(status));
		if (result != HAL_OK) {
			return result;
		}
		imu->fifo_flags = status[1] & 0xF8U;
		if (status[1] & 0x48U) { /* latched or current overrun */
			++imu->overrun_count;
		}
		uint16_t words = (uint16_t)(((status[1] & 0x03U) << 8) | status[0]);
		if (words == 0) {
			return HAL_OK;
		}
		uint16_t chunk = words < CHUNK ? words : CHUNK;
		result = read_registers(imu, 0x78, fifo_data, chunk * BYTES_PER_WORD);
		if (result != HAL_OK) {
			return result;
		}

		/* The device wraps 0x7E to 0x78 for burst reads of tagged FIFO words. */
		for (uint16_t i = 0; i < chunk * BYTES_PER_WORD; i += BYTES_PER_WORD) {
			IMU_FIFO_TAG tag = (IMU_FIFO_TAG)(fifo_data[i] >> 3);
			imu->last_timeslot = (fifo_data[i] >> 1) & 0x03U;
			switch (tag) {
			case GYROSCOPE:
				decode_xyz(&fifo_data[i + 1], imu->mdps_per_lsb, &imu->gyroscope);
				++imu->gyroscope_samples;
				break;
			case ACCELEROMETER:
				decode_xyz(&fifo_data[i + 1], imu->mg_per_lsb, &imu->accelerometer);
				++imu->accelerometer_samples;
				break;
			case TIMESTAMP:
				imu->fifo_timestamp = decode_u32(&fifo_data[i + 1])
					* imu->timestamp_resolution + imu->timestamp_offset;
				break;
			default:
				++imu->unknown_tags;
				break;
			}
		}
		drained += chunk;
	}
	return HAL_BUSY; /* Continue on the next task wakeup. */
}

HAL_StatusTypeDef IMU_log_clock_pair(imu_t *imu)
{
	if (imu == NULL || imu->i2c == NULL || !imu->initialized) {
		return HAL_ERROR;
	}
	uint8_t timestamp[4];
	HAL_StatusTypeDef result = read_registers(imu, 0x40, timestamp, sizeof(timestamp));
	if (result == HAL_OK) {
		/* Keep the pair for inspection/future synchronization; no SD dependency. */
		imu->mcu_tick = xTaskGetTickCount();
		imu->live_timestamp = decode_u32(timestamp);
	}
	return result;
}
