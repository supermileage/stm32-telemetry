/*
 * exti.c
 *
 *  Created on: Oct 12, 2026
 *      Author: Faysal
 *
 *  Callback for interrupts
 */

#include "main.h"

#include "imu_task.h"

//temporary, removed when CAN interrupt function moved to its own file
#include "can_common.h"
#include "orion_bms.h"
#include "util.h"
#include <stdio.h>
extern FDCAN_HandleTypeDef hfdcan1;

void HAL_GPIO_EXTI_Rising_Callback(uint16_t pin) {
	if (pin == Mems_ISM330DLC_INT1_Pin) {
		IMU_INT1_from_ISR();
	}
}

//FDCAN_TxHeaderTypeDef TxHeader;
FDCAN_RxHeaderTypeDef RxHeader;
//uint8_t TxData[8] = { 0 }; //init all to 0
uint8_t RxData[8];
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs){
	//block for new FIFO message
	if((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET){
		HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &RxHeader, RxData);
		printf("New packet received\n");

		switch(RxHeader.Identifier){
			case CAN_ORIONBMS_STATUS:
				printf("BMS status\n");

				uint8_t status = RxData[0];
				bool discharge_enabled = status & 0x1;
				bool charge_enabled = status & 0x2;
				if(discharge_enabled) { printf("Discharge is enabled\n"); }
				if(charge_enabled) { printf("Charge is enabled\n"); }

				uint8_t fault = parseFault(RxData);
				printf("Fault ID: %d\n", fault);
				break;
			case CAN_ORIONBMS_PACK:
				printf("BMS pack\n");

				uint8_t soc_data = RxData[4];
				float soc = soc_data / BMS_SOC_SCALING_FACTOR;
				printf("State of charge: %f%%\n", soc);

				uint16_t voltage_data = combine_u8_to_u16(RxData[0], RxData[1]);
				float voltage = voltage_data / BMS_VOLTAGE_SCALING_FACTOR;
				printf("Battery voltage: %f\n", voltage);

				uint16_t current_data = combine_u8_to_u16(RxData[2], RxData[3]);
				float current = current_data / BMS_CURRENT_SCALING_FACTOR;
				printf("Battery current: %f\n", current);
				break;
			case CAN_ORIONBMS_CELL:
				printf("BMS cell\n");

				uint16_t voltage_min_data = combine_u8_to_u16(RxData[0], RxData[1]);
				float voltage_min = voltage_min_data / BMS_CELL_SCALING_FACTOR;
				printf("Cells' voltage min: %f\n", voltage_min);

				uint16_t voltage_max_data = combine_u8_to_u16(RxData[2], RxData[3]);
				float voltage_max = voltage_max_data / BMS_CELL_SCALING_FACTOR;
				printf("Cells' voltage max: %f\n", voltage_max);

				uint16_t voltage_avg_data = combine_u8_to_u16(RxData[4], RxData[5]);
				float voltage_avg = voltage_avg_data / BMS_CELL_SCALING_FACTOR;
				printf("Cells' voltage avg: %f\n", voltage_avg);

				break;
			case CAN_ORIONBMS_TEMP:
				printf("BMS temp\n");

				uint8_t battery_temp_min = RxData[0];
				printf("Battery temperature min: %d\n", battery_temp_min);

				uint8_t battery_temp_max = RxData[1];
				printf("Battery temperature max: %d\n", battery_temp_max);

				uint8_t battery_temp_avg = RxData[2];
				printf("Battery temperature avg: %d\n", battery_temp_avg);

				uint8_t bms_temp = RxData[3];
				printf("BMS temperature: %d\n", bms_temp);

				break;
			default:
				printf("Unknown ID %lu\n", RxHeader.Identifier);
				break;
		}

	}
}
