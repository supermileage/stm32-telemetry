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

void HAL_GPIO_EXTI_Rising_Callback(uint16_t pin) {
	if (pin == Mems_ISM330DLC_INT1_Pin) {
		IMU_INT1_from_ISR();
	}
}
