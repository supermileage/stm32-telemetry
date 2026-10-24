#include "spi_bus.h"
#include "task.h"
#include <stddef.h>

static bool task_context(void)
{
    /* An ISR can't wait for a mutex held by the task it just interrupted. */
    return __get_IPSR() == 0U && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
}

static bool owns_bus(const spi_device_t *device)
{
    return task_context() && device != NULL && device->bus != NULL &&
           device->bus->mutex != NULL &&
           device->bus->active_device == device &&
           xSemaphoreGetMutexHolder(device->bus->mutex) == xTaskGetCurrentTaskHandle();
}

bool spi_bus_init(spi_bus_t *bus, SPI_HandleTypeDef *handle)
{
    if (__get_IPSR() != 0U || bus == NULL || handle == NULL ||
        handle->Instance == NULL || handle->State != HAL_SPI_STATE_READY) {
        return false;
    }
    if (bus->mutex != NULL) {
        return bus->handle == handle;
    }
    /* These are fixed CubeMX settings. */
    configASSERT(handle->Init.Mode == SPI_MODE_MASTER);
    configASSERT(handle->Init.Direction == SPI_DIRECTION_2LINES);
    configASSERT(handle->Init.DataSize == SPI_DATASIZE_8BIT);
    configASSERT(handle->Init.FirstBit == SPI_FIRSTBIT_MSB);
    configASSERT(handle->Init.NSS == SPI_NSS_SOFT);
    configASSERT(handle->Init.TIMode == SPI_TIMODE_DISABLE);
    configASSERT(handle->Init.CRCCalculation == SPI_CRCCALCULATION_DISABLE);
    bus->mutex = xSemaphoreCreateMutexStatic(&bus->mutex_storage);
    if (bus->mutex == NULL) {
        return false;
    }
    bus->handle = handle;
    bus->active_device = NULL;
    return true;
}

bool spi_acquire(const spi_device_t *device, TickType_t wait_ticks)
{
    if (!task_context() || device == NULL || device->bus == NULL ||
        device->bus->handle == NULL || device->bus->mutex == NULL ||
        device->cs_port == NULL || device->cs_pin == 0U ||
        (device->cs_pin & (device->cs_pin - 1U)) != 0U) {
        return false;
    }
    spi_bus_t *bus = device->bus;
    /* Taking our own non-recursive mutex would make us wait on ourselves. */
    if (xSemaphoreGetMutexHolder(bus->mutex) == xTaskGetCurrentTaskHandle() ||
        xSemaphoreTake(bus->mutex, wait_ticks) != pdTRUE) {
        return false;
    }
    HAL_GPIO_WritePin(device->cs_port, device->cs_pin, GPIO_PIN_SET);
    bus->active_device = device;
    return true;
}

bool spi_select(const spi_device_t *device, bool selected)
{
    if (!owns_bus(device)) {
        return false;
    }
    HAL_GPIO_WritePin(device->cs_port, device->cs_pin,
                      selected ? GPIO_PIN_RESET : GPIO_PIN_SET);
    return true;
}

bool spi_exchange(const spi_device_t *device, const uint8_t *tx, uint8_t *rx,
                  uint16_t size, uint32_t timeout_ms)
{
    if (!owns_bus(device) || tx == NULL || rx == NULL || size == 0U ||
        device->bus->handle->State != HAL_SPI_STATE_READY) {
        return false;
    }
    return HAL_SPI_TransmitReceive(device->bus->handle, tx, rx, size, timeout_ms) == HAL_OK;
}

void spi_release(const spi_device_t *device)
{
    if (!owns_bus(device)) {
        return;
    }
    HAL_GPIO_WritePin(device->cs_port, device->cs_pin, GPIO_PIN_SET);
    device->bus->active_device = NULL;
    (void)xSemaphoreGive(device->bus->mutex);
}
