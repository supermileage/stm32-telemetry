#include "queue.h"
#include "rtos_mock.h"
#include <assert.h>
#include <string.h>
TickType_t mock_tick;
TaskFunction_t mock_task_entry;
unsigned int mock_task_creations;
bool mock_task_creation_fails;
void (*mock_receive_hook)(TickType_t wait);
void (*mock_delay_hook)(void);

TaskHandle_t xTaskCreateStatic(TaskFunction_t entry, const char *name,
    uint32_t depth, void *argument, UBaseType_t priority,
    StackType_t *stack, StaticTask_t *control)
{
    (void)name; (void)argument; (void)priority;
    assert(depth == 1024U && stack != NULL && control != NULL);
    ++mock_task_creations;
    mock_task_entry = entry;
    return mock_task_creation_fails ? NULL : control;
}
TickType_t xTaskGetTickCount(void) { return mock_tick; }
void vTaskDelay(TickType_t wait)
{
    mock_tick += wait;
    if (mock_delay_hook != NULL) mock_delay_hook();
}
QueueHandle_t xQueueCreateStatic(UBaseType_t length, UBaseType_t size,
    uint8_t *storage, StaticQueue_t *control)
{
    *control = (StaticQueue_t){ .storage = storage, .capacity = length, .size = size };
    return control;
}
BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t wait)
{
    assert(wait == 0U);
    if (queue->count == queue->capacity) return pdFALSE;
    memcpy(queue->storage + ((queue->head + queue->count) % queue->capacity) * queue->size,
        item, queue->size);
    ++queue->count;
    return pdPASS;
}
BaseType_t xQueueSendFromISR(QueueHandle_t queue, const void *item, BaseType_t *woken)
{
    BaseType_t result = xQueueSend(queue, item, 0U);
    if (result == pdPASS) *woken = pdTRUE;
    return result;
}
BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t wait)
{
    if (mock_receive_hook != NULL) mock_receive_hook(wait);
    if (queue->count == 0U) return pdFALSE;
    memcpy(item, queue->storage + queue->head * queue->size, queue->size);
    queue->head = (queue->head + 1U) % queue->capacity;
    --queue->count;
    return pdPASS;
}
BaseType_t xQueueReset(QueueHandle_t queue)
{
    queue->count = queue->head = 0U;
    return pdPASS;
}
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue) { return queue->count; }
