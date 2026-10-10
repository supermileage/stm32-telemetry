#ifndef TEST_TASK_H
#define TEST_TASK_H
#include "FreeRTOS.h"
/* Single-threaded mock. Scheduling/races require on-target verification. */
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#define taskENTER_CRITICAL_FROM_ISR() (0U)
#define taskEXIT_CRITICAL_FROM_ISR(mask) ((void)(mask))
typedef void (*TaskFunction_t)(void *argument);
TaskHandle_t xTaskCreateStatic(TaskFunction_t entry, const char *name,
    uint32_t depth, void *argument, UBaseType_t priority,
    StackType_t *stack, StaticTask_t *control);
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t wait);
#endif
