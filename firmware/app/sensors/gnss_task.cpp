#include "gnss_task.h"
#include "gnss_parser.hpp"
#include "logging/logger.h"
#include "uart/uart_port.h"

#include "task.h"

#include <stddef.h>

enum {
    /** FreeRTOS counts stack entries. This reserves 2 KiB on this MCU. */
    GNSS_STACK_DEPTH = 512U,
    GNSS_READ_SIZE = 16U,
    GNSS_READ_TIMEOUT_MS = 20U,
    GNSS_RX_RETRY_MS = 100U
};

static uart_port_t gnss_uart;
static GnssParser parser;
static StaticTask_t task_control;
static StackType_t task_stack[GNSS_STACK_DEPTH];
static TaskHandle_t task_handle;

/** Critical sections keep counter updates together for gnss_get_status(). */
static gnss_status_t status;

static void gnss_task(void *argument)
{
    (void)argument;

    uint8_t bytes[GNSS_READ_SIZE];
    uart_rx_status_t previous_rx = uart_port_rx_status(&gnss_uart);

    for (;;) {
        /** Retry reception after startup failures or UART errors. */
        if (!uart_port_start_rx(&gnss_uart)) {
            parser.reset();
            previous_rx = uart_port_rx_status(&gnss_uart);
            vTaskDelay(pdMS_TO_TICKS(GNSS_RX_RETRY_MS));
            continue;
        }

        size_t count = uart_port_read(&gnss_uart, bytes, GNSS_READ_SIZE,
                                     pdMS_TO_TICKS(GNSS_READ_TIMEOUT_MS));
        uart_rx_status_t rx = uart_port_rx_status(&gnss_uart);
        bool damaged = rx.dropped_bytes != previous_rx.dropped_bytes ||
                       rx.errors != previous_rx.errors || !rx.active;
        previous_rx = rx;

        if (damaged) {
            /** Lost or damaged bytes invalidate the partial sentence. */
            parser.reset();
            continue;
        }

        for (size_t i = 0U; i < count; ++i) {
            gnss_sample_t sample;
            gnss_parse_result_t result = parser.feed(bytes[i], sample);
            if (result == GNSS_PARSE_NONE) {
                continue;
            }

            bool accepted = false;
            bool rtc_initialized = false;
            bool rtc_failed = false;

            if (result == GNSS_PARSE_RECORD) {
                if (sample.utc_valid && !sample_clock_utc_is_set()) {
                    rtc_initialized = sample_clock_init_utc(&sample.utc);
                    rtc_failed = !rtc_initialized;
                }

                log_record_t record{};
                /** Keep this capture time with the measurement through SD delays. */
                record.timestamp = sample_clock_now();
                record.source = LOG_SOURCE_GPS;
                record.data.gps = sample.measurement;

                /** A full logger queue must not hold up UART reception. */
                accepted = logger_submit(&record);
            }

            taskENTER_CRITICAL();
            if (result == GNSS_PARSE_RECORD) {
                ++status.parsed_records;
                if (rtc_initialized) {
                    ++status.rtc_initializations;
                }
                if (rtc_failed) {
                    ++status.rtc_init_failures;
                }
                if (!sample.utc_valid) {
                    ++status.invalid_utc_sentences;
                }
                if (!accepted) {
                    ++status.rejected_records;
                }
            } else if (result == GNSS_PARSE_INVALID) {
                ++status.invalid_sentences;
            } else if (result == GNSS_PARSE_OVERLONG) {
                ++status.overlong_sentences;
            }
            taskEXIT_CRITICAL();
        }
    }
}

bool gnss_start_task(UART_HandleTypeDef *uart)
{
    if (__get_IPSR() != 0U || xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        return false;
    }

    if (task_handle != NULL) {
        return gnss_uart.handle == uart;
    }

    if (uart == NULL || (uart->Init.Mode & UART_MODE_RX) == 0U ||
        !uart_port_init(&gnss_uart, uart)) {
        return false;
    }

    task_handle = xTaskCreateStatic(gnss_task, "gnss", GNSS_STACK_DEPTH, NULL,
                                  tskIDLE_PRIORITY + 2U, task_stack, &task_control);
    return task_handle != NULL;
}

void gnss_get_status(gnss_status_t *out)
{
    if (out == NULL) {
        return;
    }

    taskENTER_CRITICAL();
    *out = status;
    taskEXIT_CRITICAL();

    uart_rx_status_t rx = uart_port_rx_status(&gnss_uart);
    out->rx_active = rx.active;
    out->rx_dropped_bytes = rx.dropped_bytes;
    out->rx_errors = rx.errors;
}
