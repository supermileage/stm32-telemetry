#include "gnss_task.h"
#include "gnss_parser.h"
#include "logging/logger.h"
#include "uart/uart_port.h"
#include "task.h"

#include <stddef.h>

enum {
    GNSS_STACK_DEPTH = 512U,   /* StackType_t entries: 2 KiB on this MCU. */
    GNSS_READ_SIZE = 16U,
    GNSS_READ_TIMEOUT_MS = 20U,
    GNSS_RX_RETRY_MS = 100U
};

static uart_port_t gnss_uart;
static StaticTask_t task_control;
static StackType_t task_stack[GNSS_STACK_DEPTH];
static TaskHandle_t task_handle;
/* Task counters are protected so gnss_get_status() can read them consistently. */
static gnss_status_t status;

static void gnss_task(void *argument)
{
    (void)argument;
    uart_rx_byte_t bytes[GNSS_READ_SIZE];
    uart_rx_status_t previous_rx = uart_port_rx_status(&gnss_uart);

    for (;;) {
        /* Keep retrying reception after a startup failure or UART error. */
        if (!uart_port_start_rx(&gnss_uart)) {
            gnss_parser_reset();
            previous_rx = uart_port_rx_status(&gnss_uart);
            vTaskDelay(pdMS_TO_TICKS(GNSS_RX_RETRY_MS));
            continue;
        }

        size_t count = uart_port_read_timed(&gnss_uart, bytes, GNSS_READ_SIZE,
                                           pdMS_TO_TICKS(GNSS_READ_TIMEOUT_MS));
        uart_rx_status_t rx = uart_port_rx_status(&gnss_uart);
        bool damaged = rx.dropped_bytes != previous_rx.dropped_bytes ||
                       rx.errors != previous_rx.errors || !rx.active;
        previous_rx = rx;
        if (damaged) {
            /* Resume assembly at a later '$' after damaged or missing input. */
            gnss_parser_reset();
            continue;
        }

        for (size_t i = 0U; i < count; ++i) {
            log_record_t record;
            gnss_parse_result_t result = gnss_parser_feed(bytes[i].byte,
                                                        bytes[i].tick_ms, &record);
            if (result == GNSS_PARSE_NONE) {
                continue;
            }
            bool accepted = false;
            if (result == GNSS_PARSE_RECORD) {
                /* Nonblocking: storage trouble must not stop UART reception. */
                accepted = logger_submit(&record);
            }

            gnss_parser_stats_t parser_stats = gnss_parser_get_stats();
            taskENTER_CRITICAL();
            if (result == GNSS_PARSE_RECORD) {
                ++status.parsed_records;
                if (!accepted) {
                    ++status.rejected_records;
                }
            } else if (result == GNSS_PARSE_INVALID) {
                ++status.invalid_sentences;
            } else if (result == GNSS_PARSE_OVERLONG) {
                ++status.overlong_sentences;
            }
            status.utc_updates = parser_stats.utc_updates;
            status.rejected_utc_updates = parser_stats.rejected_utc_updates;
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
