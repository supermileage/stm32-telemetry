#ifndef TEST_QUEUE_H
#define TEST_QUEUE_H
#include "FreeRTOS.h"
typedef struct {
    uint8_t *storage;
    UBaseType_t capacity, size, head, count;
} StaticQueue_t;
typedef StaticQueue_t *QueueHandle_t;
QueueHandle_t xQueueCreateStatic(UBaseType_t length, UBaseType_t size,
    uint8_t *storage, StaticQueue_t *control);
BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t wait);
BaseType_t xQueueSendFromISR(QueueHandle_t queue, const void *item, BaseType_t *woken);
BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t wait);
BaseType_t xQueueReset(QueueHandle_t queue);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue);
#endif
