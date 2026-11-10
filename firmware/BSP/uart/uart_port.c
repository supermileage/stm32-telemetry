#include "uart_port.h"
#include "usart.h"
#include "task.h"

_Static_assert(USE_HAL_UART_REGISTER_CALLBACKS == 0U,
               "uart_port owns the project's global HAL UART RX callbacks.");

typedef struct {
    UART_HandleTypeDef *handle;
    IRQn_Type irq;
    uart_port_t *port;
} uart_binding_t;

/* Owners are published during serial startup and remain fixed for this boot. */
static uart_binding_t bindings[] = {
    {&huart1, USART1_IRQn, NULL},
    {&huart3, USART3_IRQn, NULL},
    {&huart4, UART4_IRQn, NULL}
};

static uart_binding_t *binding_for(UART_HandleTypeDef *handle)
{
    for (size_t i = 0U; i < sizeof(bindings) / sizeof(bindings[0]); ++i) {
        if (bindings[i].handle == handle) {
            return &bindings[i];
        }
    }

    return NULL;
}

static bool is_task_context(void)
{
    return __get_IPSR() == 0U && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
}

bool uart_port_init(uart_port_t *port, UART_HandleTypeDef *handle)
{
    if (__get_IPSR() != 0U || xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED ||
        port == NULL || handle == NULL) {
        return false;
    }

    if (port->rx_queue != NULL) {
        return port->handle == handle;
    }

    uart_binding_t *binding = binding_for(handle);
    if (binding == NULL || binding->port != NULL || handle->Instance == NULL ||
        handle->gState != HAL_UART_STATE_READY || handle->RxState != HAL_UART_STATE_READY) {
        return false;
    }

    if (handle->Init.WordLength != UART_WORDLENGTH_8B ||
        handle->Init.Parity != UART_PARITY_NONE || handle->FifoMode != UART_FIFOMODE_DISABLE) {
        return false;
    }

    /* All implemented priority bits must be preemption bits, as HAL_Init sets. */
    configASSERT(NVIC_GetPriorityGrouping() == NVIC_PRIORITYGROUP_4);

    QueueHandle_t queue = xQueueCreateStatic(UART_PORT_RX_BUFFER_SIZE,
        sizeof(uart_rx_byte_t), port->rx_storage, &port->rx_control);
    SemaphoreHandle_t mutex = xSemaphoreCreateMutexStatic(&port->tx_mutex_storage);
    if (queue == NULL || mutex == NULL) {
        return false;
    }

    port->handle = handle;
    port->rx_queue = queue;
    port->tx_mutex = mutex;
    binding->port = port;

    return true;
}

bool uart_port_start_rx(uart_port_t *port)
{
    if (!is_task_context() || port == NULL || port->rx_queue == NULL ||
        (port->handle->Init.Mode & UART_MODE_RX) == 0U) {
        return false;
    }

    const uart_binding_t *binding = binding_for(port->handle);
    if (binding == NULL || NVIC_GetEnableIRQ(binding->irq) == 0U ||
        NVIC_GetPriority(binding->irq) < configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY) {
        return false;
    }

    /* Receive_IT only sets state/registers. Mask the UART IRQ until rx_active
     * is published, including when an already pending byte arrives immediately. */
    taskENTER_CRITICAL();
    if (!port->rx_active) {
        port->rx_active = HAL_UART_Receive_IT(port->handle, &port->rx_byte, 1U) == HAL_OK;
        if (!port->rx_active) {
            ++port->rx_errors;
        }
    }

    bool active = port->rx_active;
    taskEXIT_CRITICAL();

    return active;
}

size_t uart_port_read(uart_port_t *port, uint8_t *destination, size_t capacity,
                     TickType_t timeout_ticks)
{
    if (!is_task_context() || port == NULL || port->rx_queue == NULL ||
        destination == NULL || capacity == 0U) {
        return 0U;
    }

    size_t count = 0U;
    uart_rx_byte_t received;
    while (count < capacity &&
           xQueueReceive(port->rx_queue, &received, count == 0U ? timeout_ticks : 0U) == pdPASS) {
        destination[count++] = received.byte;
    }
    return count;
}

size_t uart_port_read_timed(uart_port_t *port, uart_rx_byte_t *destination,
                           size_t capacity, TickType_t timeout_ticks)
{
    if (!is_task_context() || port == NULL || port->rx_queue == NULL ||
        destination == NULL || capacity == 0U) {
        return 0U;
    }

    size_t count = 0U;
    while (count < capacity &&
           xQueueReceive(port->rx_queue, &destination[count],
                         count == 0U ? timeout_ticks : 0U) == pdPASS) {
        ++count;
    }
    return count;
}

bool uart_port_write(uart_port_t *port, const uint8_t *data, uint16_t length,
                     TickType_t wait_ticks, uint32_t timeout_ms)
{
    if (!is_task_context() || port == NULL || port->tx_mutex == NULL ||
        data == NULL || length == 0U || (port->handle->Init.Mode & UART_MODE_TX) == 0U) {
        return false;
    }

    if (xSemaphoreTake(port->tx_mutex, wait_ticks) != pdTRUE) {
        return false;
    }

    bool written = HAL_UART_Transmit(port->handle, data, length, timeout_ms) == HAL_OK;
    (void)xSemaphoreGive(port->tx_mutex);

    return written;
}

uart_rx_status_t uart_port_rx_status(const uart_port_t *port)
{
    uart_rx_status_t status = {0};

    if (!is_task_context() || port == NULL || port->rx_queue == NULL) {
        return status;
    }

    taskENTER_CRITICAL();
    status.active = port->rx_active;
    status.dropped_bytes = port->rx_dropped_bytes;
    status.errors = port->rx_errors;
    taskEXIT_CRITICAL();

    return status;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *handle)
{
    const uart_binding_t *binding = binding_for(handle);
    if (binding == NULL || binding->port == NULL || !binding->port->rx_active) {
        return;
    }

    uart_port_t *port = binding->port;

    /* HAL may report completion before its error callback. Rearming here would
     * clear ErrorCode and hide the error. Leave recovery to ErrorCallback. */
    if (handle->ErrorCode != HAL_UART_ERROR_NONE) {
        return;
    }

    BaseType_t woken = pdFALSE;
    uart_rx_byte_t received = {
        .tick_ms = HAL_GetTick(),
        .byte = port->rx_byte
    };
    if (xQueueSendFromISR(port->rx_queue, &received, &woken) != pdPASS) {
        ++port->rx_dropped_bytes;
    }

    port->rx_active = HAL_UART_Receive_IT(handle, &port->rx_byte, 1U) == HAL_OK;
    if (!port->rx_active) {
        ++port->rx_errors;
    }

    portYIELD_FROM_ISR(woken);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *handle)
{
    const uart_binding_t *binding = binding_for(handle);
    if (binding == NULL || binding->port == NULL || !binding->port->rx_active) {
        return;
    }

    uart_port_t *port = binding->port;
    ++port->rx_errors;

    /* HAL clears the hardware error flags. Restart only if it ended RX;
     * noise/framing can leave RX running. Count recovery as part of this error. */
    if (handle->RxState == HAL_UART_STATE_READY) {
        port->rx_active = HAL_UART_Receive_IT(handle, &port->rx_byte, 1U) == HAL_OK;
    }
}
