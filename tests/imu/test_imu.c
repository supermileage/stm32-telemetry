#include "ism330dhcx.h"
#include "imu_task.h"
#include "task.h"
#include "main.h"
#include <assert.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

static I2C_HandleTypeDef bus;
UART_HandleTypeDef huart4;
static uint8_t registers[256];
static uint8_t fifo[512 * BYTES_PER_WORD];
static unsigned int fifo_words, fifo_index, fifo_reads, largest_read;
static unsigned int operations, fail_operation;
static uint8_t status_flags;
static int never_empty;
static TickType_t tick;
static unsigned int notifications, yields, waits;
static char uart_output[512];
static size_t uart_size;
static jmp_buf task_exit;
void HAL_GPIO_EXTI_Rising_Callback(uint16_t pin);

static void reset_mock(void)
{
    memset(registers, 0, sizeof(registers));
    memset(fifo, 0, sizeof(fifo));
    registers[0x0F] = 0x6B;
    fifo_words = fifo_index = fifo_reads = largest_read = 0;
    operations = fail_operation = 0;
    status_flags = 0;
    never_empty = 0;
    tick = 100;
    notifications = yields = waits = 0;
    uart_size = 0;
    uart_output[0] = '\0';
}

static void push_xyz(uint8_t tag, int16_t x, int16_t y, int16_t z)
{
    assert(fifo_words < 512);
    uint8_t *word = &fifo[fifo_words++ * BYTES_PER_WORD];
    word[0] = (uint8_t)((tag << 3) | (3U << 1));
    const int16_t values[] = {x, y, z};
    for (unsigned int i = 0; i < 3; ++i) {
        word[1 + 2 * i] = (uint8_t)values[i];
        word[2 + 2 * i] = (uint8_t)((uint16_t)values[i] >> 8);
    }
}

static int io_fails(I2C_HandleTypeDef *i2c, uint16_t address, uint16_t width, uint32_t timeout)
{
    assert(i2c == &bus && address == 0xD6 && width == I2C_MEMADD_SIZE_8BIT);
    assert(timeout == 200);
    return ++operations == fail_operation;
}

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *i2c, uint16_t address,
    uint16_t reg, uint16_t width, uint8_t *data, uint16_t size, uint32_t timeout)
{
    if (io_fails(i2c, address, width, timeout)) return HAL_TIMEOUT;
    if (reg == 0x3A) {
        assert(size == 2);
        unsigned int count = never_empty ? 512 : fifo_words - fifo_index;
        data[0] = (uint8_t)count;
        data[1] = (uint8_t)((count >> 8) | status_flags);
        status_flags = 0;
    } else if (reg == 0x78) {
        assert(size % BYTES_PER_WORD == 0 && size <= CHUNK * BYTES_PER_WORD);
        ++fifo_reads;
        if (size > largest_read) largest_read = size;
        if (never_empty) memset(data, 0, size);
        else {
            assert(fifo_index + size / BYTES_PER_WORD <= fifo_words);
            memcpy(data, &fifo[fifo_index * BYTES_PER_WORD], size);
            fifo_index += size / BYTES_PER_WORD;
        }
    } else {
        assert(reg + size <= sizeof(registers));
        memcpy(data, &registers[reg], size);
    }
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *i2c, uint16_t address,
    uint16_t reg, uint16_t width, uint8_t *data, uint16_t size, uint32_t timeout)
{
    if (io_fails(i2c, address, width, timeout)) return HAL_TIMEOUT;
    assert(reg + size <= sizeof(registers));
    memcpy(&registers[reg], data, size);
    if (reg == 0x0A && data[0] == 0) fifo_words = fifo_index = 0;
    return HAL_OK;
}

TickType_t xTaskGetTickCount(void) { return tick; }
void vTaskDelay(TickType_t ticks) { tick += ticks; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return &bus; }
void vTaskNotifyGiveFromISR(TaskHandle_t handle, BaseType_t *wake)
{
    assert(handle == &bus);
    ++notifications;
    *wake = pdTRUE;
}
void mock_yield_from_isr(BaseType_t wake)
{
    assert(wake == pdTRUE);
    ++yields;
}
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t timeout)
{
    assert(clear == pdTRUE && timeout == pdMS_TO_TICKS(100));
    if (++waits == 2) longjmp(task_exit, 1);
    tick += timeout;
    HAL_GPIO_EXTI_Rising_Callback(1); /* Unrelated EXTI must not notify this task. */
    assert(notifications == 0);
    HAL_GPIO_EXTI_Rising_Callback(Mems_ISM330DLC_INT1_Pin);
    push_xyz(GYROSCOPE, 2000, -2000, 0);
    push_xyz(ACCELEROMETER, 1000, -1000, 0);
    return 1;
}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *uart, uint8_t *data,
    uint16_t size, uint32_t timeout)
{
    assert(uart == &huart4 && timeout == 200);
    assert(uart_size + size < sizeof(uart_output));
    memcpy(uart_output + uart_size, data, size);
    uart_size += size;
    uart_output[uart_size] = '\0';
    return HAL_OK;
}

static void test_fifo(void)
{
    reset_mock();
    registers[0x63] = (uint8_t)-10; /* Signed factory clock correction. */
    registers[0x40] = 100;
    imu_t imu = {.i2c = &bus};
    assert(IMU_init(&imu) == HAL_OK && imu.initialized);
    assert(tick == 135); /* Startup delay also applies when scheduler is already running. */
    assert(fabs(imu.timestamp_resolution - 0.000025 / 0.985) < 1e-12);
    assert(registers[0x10] == 0x48 && registers[0x11] == 0x44);
    assert(registers[0x12] == 0x44 && registers[0x0A] == 0xC6);
    assert(IMU_drain_fifo(&imu) == HAL_OK && fifo_reads == 0);
    /* Cross the low-byte FIFO level boundary and many 32-word burst boundaries. */
    for (unsigned int i = 0; i < 260; ++i) push_xyz(GYROSCOPE, 32767, -32768, 2000);
    push_xyz(ACCELEROMETER, 1000, -1000, 0);
    push_xyz(TIMESTAMP, 200, 0, 0);
    push_xyz(0x1F, 0, 0, 0);
    status_flags = 0x48;
    assert(IMU_drain_fifo(&imu) == HAL_OK);
    assert(imu.gyroscope_samples == 260 && imu.accelerometer_samples == 1);
    assert(imu.gyroscope.x == 573422 && imu.gyroscope.y == -573440);
    assert(imu.gyroscope.z == 35000); /* Must not truncate to int16_t. */
    assert(imu.accelerometer.x == 122 && imu.accelerometer.y == -122);
    assert(imu.last_timeslot == 3 && imu.unknown_tags == 1 && imu.overrun_count == 1);
    assert(fifo_index == fifo_words && largest_read == 224 && fifo_reads == 9);
    assert(fabs(imu.fifo_timestamp - (0.135 + 100 * imu.timestamp_resolution)) < 1e-10);
    registers[0x40] = 0x78; registers[0x41] = 0x56;
    registers[0x42] = 0x34; registers[0x43] = 0x12;
    tick = 1000;
    assert(IMU_log_clock_pair(&imu) == HAL_OK);
    assert(imu.live_timestamp == 0x12345678 && imu.mcu_tick == 1000);
    never_empty = 1;
    unsigned int before = fifo_reads;
    assert(IMU_drain_fifo(&imu) == HAL_BUSY && fifo_reads - before == 16);
}

static void test_errors(void)
{
    reset_mock();
    imu_t imu = {.i2c = &bus};
    assert(IMU_init(&imu) == HAL_OK);
    unsigned int init_operations = operations;
    /* Fail each bus transaction, including the final FIFO-enable write. */
    for (unsigned int i = 1; i <= init_operations; ++i) {
        reset_mock();
        fail_operation = i;
        assert(IMU_init(&imu) == HAL_TIMEOUT && !imu.initialized);
        assert(IMU_drain_fifo(&imu) == HAL_ERROR);
    }
    reset_mock();
    registers[0x0F] = 0;
    assert(IMU_init(&imu) == HAL_ERROR && operations == 1);
    reset_mock();
    assert(IMU_init(&imu) == HAL_OK);
    push_xyz(GYROSCOPE, 1234, 5678, 0);
    fail_operation = operations + 1; /* Failed level read: no FIFO access. */
    assert(IMU_drain_fifo(&imu) == HAL_TIMEOUT && fifo_reads == 0);
    fail_operation = operations + 2; /* Failed payload read: no decoding. */
    assert(IMU_drain_fifo(&imu) == HAL_TIMEOUT && imu.gyroscope_samples == 0);
    fail_operation = operations + 1;
    uint32_t previous = imu.live_timestamp;
    assert(IMU_log_clock_pair(&imu) == HAL_TIMEOUT && imu.live_timestamp == previous);
    fail_operation = 0;
    assert(IMU_drain_fifo(&imu) == HAL_OK && imu.gyroscope_samples == 1);
    assert(IMU_init(NULL) == HAL_ERROR && IMU_drain_fifo(NULL) == HAL_ERROR);
    assert(IMU_log_clock_pair(NULL) == HAL_ERROR);
}

static void test_task(void)
{
    reset_mock();
    IMU_INT1_from_ISR(); /* IRQ before task startup is harmless. */
    assert(notifications == 0 && yields == 0);
    if (setjmp(task_exit) == 0) IMU_task(&bus);
    assert(notifications == 1 && yields == 1 && waits == 2);
    assert(strstr(uart_output, "gyroscope (mdps): x:+35000, y:-35000, z:+0") != NULL);
    assert(strstr(uart_output, "accelerometer (mg): x:+122, y:-122, z:+0") != NULL);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (strcmp(argv[1], "fifo") == 0) test_fifo();
    else if (strcmp(argv[1], "errors") == 0) test_errors();
    else if (strcmp(argv[1], "task") == 0) test_task();
    else assert(!"unknown scenario");
    puts("IMU scenario passed");
    return 0;
}
