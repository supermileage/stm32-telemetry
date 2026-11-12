#include "fcc_task.hpp"
#include "fcc_parser.hpp"
#include "logging/logger.h"
#include "uart/uart_port.h"
#include "task.h"

namespace {
constexpr uint32_t StackDepth = 512U; // 2 KiB on STM32; verify stack margin.
constexpr std::size_t ReadCapacity = 2U * fcc_protocol::PacketSize;
constexpr uint32_t ReadTimeoutMs = 10U;
constexpr uint32_t RxRetryMs = 100U;
constexpr UBaseType_t TaskPriority = tskIDLE_PRIORITY + 2U;

uart_port_t port;
StaticTask_t task_control;
StackType_t task_stack[StackDepth];
TaskHandle_t task_handle;
uint32_t configured_gap_ms;
fcc_status_t status;

void process_result(FccParseResult result, const fcc_measurement_t &measurement)
{
    if (result == FccParseResult::Record) {
        log_record_t record{};
        // Processing time includes UART buffering and the ending idle wait.
        record.timestamp = sample_clock_now();
        record.source = LOG_SOURCE_FCC;
        record.data.fcc.error_flag = measurement.error_flag;
        record.data.fcc.ambient_temperature_mc = measurement.ambient_temperature_mc;
        record.data.fcc.fuel_cell_voltage_mv = measurement.fuel_cell_voltage_mv;
        record.data.fcc.hydrogen_leak_voltage_mv = measurement.hydrogen_leak_voltage_mv;
        record.data.fcc.fuel_cell_temperature_mc = measurement.fuel_cell_temperature_mc;
        record.data.fcc.fuel_cell_current_ma = measurement.fuel_cell_current_ma;
        record.data.fcc.battery_voltage_mv = measurement.battery_voltage_mv;
        const bool accepted = logger_submit(&record);
        taskENTER_CRITICAL();
        ++status.parsed_packets;
        if (!accepted) {
            ++status.rejected_records;
        }
        taskEXIT_CRITICAL();
    } else if (result == FccParseResult::Invalid) {
        taskENTER_CRITICAL();
        ++status.framing_errors;
        taskEXIT_CRITICAL();
    }
}

void fcc_task(void *)
{
    FccParser parser(configured_gap_ms);
    uart_rx_byte_t bytes[ReadCapacity];
    uart_rx_status_t previous_rx = uart_port_rx_status(&port);
    const TickType_t read_timeout = pdMS_TO_TICKS(ReadTimeoutMs) == 0U
        ? 1U : pdMS_TO_TICKS(ReadTimeoutMs);
    const TickType_t retry_delay = pdMS_TO_TICKS(RxRetryMs) == 0U
        ? 1U : pdMS_TO_TICKS(RxRetryMs);

    for (;;) {
        if (!uart_port_start_rx(&port)) {
            parser.reset();
            previous_rx = uart_port_rx_status(&port);
            taskENTER_CRITICAL();
            ++status.stream_resets;
            taskEXIT_CRITICAL();
            vTaskDelay(retry_delay);
            continue;
        }
        std::size_t count = uart_port_read_timed(&port, bytes, ReadCapacity,
                                                read_timeout);
        fcc_measurement_t measurement{};
        FccParseResult idle_result = FccParseResult::None;

        taskENTER_CRITICAL();
        if (count == 0U) {
            // More bytes may have arrived during a task switch after timeout.
            // Recheck without waiting before deciding that a burst has ended.
            count = uart_port_read_timed(&port, bytes, ReadCapacity, 0U);
        }
        const uart_rx_status_t rx = uart_port_rx_status(&port);
        const bool damaged = rx.dropped_bytes != previous_rx.dropped_bytes ||
            rx.errors != previous_rx.errors || !rx.active;
        previous_rx = rx;
        if (damaged) {
            parser.reset();
            ++status.stream_resets;
        } else if (count == 0U) {
            // Queue check, time capture, and poll are atomic against UART IRQ.
            // Pure parser only: no logger calls or hardware waits in this lock.
            idle_result = parser.poll(HAL_GetTick(), measurement);
        }
        taskEXIT_CRITICAL();

        if (damaged) {
            // Discard this uncertain chunk; resynchronize on later silence.
            continue;
        }
        process_result(idle_result, measurement);
        for (std::size_t i = 0U; i < count; ++i) {
            const FccParseResult result = parser.feed(bytes[i].byte,
                                                       bytes[i].tick_ms,
                                                       measurement);
            process_result(result, measurement);
        }
    }
}

} // namespace

bool fcc_start_task(UART_HandleTypeDef *uart, uint32_t gap_ms)
{
    if (__get_IPSR() != 0U || xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED ||
        uart == nullptr || gap_ms == 0U || gap_ms >= fcc_protocol::PacketPeriodMs) {
        return false;
    }
    if (task_handle != nullptr) {
        return port.handle == uart && configured_gap_ms == gap_ms;
    }
    if ((uart->Init.Mode & UART_MODE_RX) == 0U || !uart_port_init(&port, uart)) {
        return false;
    }
    configured_gap_ms = gap_ms;
    task_handle = xTaskCreateStatic(fcc_task, "fcc", StackDepth, nullptr,
                                    TaskPriority, task_stack, &task_control);
    return task_handle != nullptr;
}

void fcc_get_status(fcc_status_t *out)
{
    if (out == nullptr) {
        return;
    }
    taskENTER_CRITICAL();
    *out = status;
    taskEXIT_CRITICAL();
    const uart_rx_status_t rx = uart_port_rx_status(&port);
    out->rx_dropped_bytes = rx.dropped_bytes;
    out->rx_errors = rx.errors;
    out->rx_active = rx.active;
}
