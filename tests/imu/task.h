#ifndef TEST_IMU_TASK_H
#define TEST_IMU_TASK_H
#include "FreeRTOS.h"
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t ticks);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t timeout);
void vTaskNotifyGiveFromISR(TaskHandle_t handle, BaseType_t *wake);
#endif
