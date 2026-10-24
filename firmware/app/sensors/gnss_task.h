/**
 * @file
 * @brief Startup interface for the GNSS UART receive task.
 */
# pragma once

#include "usart.h"


/**
 * @brief Start the GNSS receive task on its UART.
 *
 * Configures the UART for GNSS comms, prepares the receive buffer,
 * and creates the task that processes incoming GNSS data.
 *
 * @param huart Pointer to the UART handle connected to the GNSS receiver.
 */
void gnss_start_task(UART_HandleTypeDef *huart);
