/**
 * @file
 * @brief Reserved startup interface for the unimplemented GNSS task.
 */
#pragma once

#include "stm32u5xx_hal.h"

/**
 * @brief Reserved entry point for GNSS task startup.
 * @param huart UART handle intended for the GNSS receiver.
 */
void gnss_start_task(UART_HandleTypeDef *huart);
