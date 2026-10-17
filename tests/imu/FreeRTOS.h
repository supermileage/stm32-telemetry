#ifndef TEST_IMU_FREERTOS_H
#define TEST_IMU_FREERTOS_H
#include <stdint.h>
#include <stddef.h>
typedef int BaseType_t;
typedef uint32_t TickType_t;
typedef void *TaskHandle_t;
#define pdTRUE 1
#define pdFALSE 0
#define configTICK_RATE_HZ 1000U
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
void mock_yield_from_isr(BaseType_t wake);
#define portYIELD_FROM_ISR(wake) mock_yield_from_isr(wake)
#endif
