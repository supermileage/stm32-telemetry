#ifndef RTOS_MOCK_H
#define RTOS_MOCK_H
#include "task.h"
#include <stdbool.h>
extern TickType_t mock_tick;
extern TaskFunction_t mock_task_entry;
extern unsigned int mock_task_creations;
extern bool mock_task_creation_fails;
extern void (*mock_receive_hook)(TickType_t wait);
extern void (*mock_delay_hook)(void);
#endif
