#include "logger.h"
#include "csv_file.hpp"
#include "csv_format.h"
#include "storage/storage.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <type_traits>

namespace {

/** Sharing one queue makes a stop wait for every earlier record. */
enum class CommandKind { Record, Stop };

struct LoggerCommand {
    CommandKind kind;
    log_record_t record;
};

static_assert(std::is_trivially_copyable_v<LoggerCommand>,
              "FreeRTOS queues copy commands as bytes");

struct SensorLog {
    sensor_source_t source;
    const char *filename;
    const char *header;
    int (*format)(const log_record_t *record, char *out, size_t capacity);
    CsvFile &file;
};

CsvFile gps_file;
CsvFile imu_file;

/** Each sensor owns a writable file buffer. This table stays read-only. */
const SensorLog sensor_files[] = {
    {LOG_SOURCE_GPS, "GPS.CSV", GNSS_CSV_HEADER, gnss_csv_format, gps_file},
    {LOG_SOURCE_IMU, "IMU.CSV", IMU_CSV_HEADER, imu_csv_format, imu_file}
};

char session_path[LOGGER_PATH_SIZE];

StaticQueue_t queue_control;
uint8_t queue_storage[LOGGER_QUEUE_DEPTH * sizeof(LoggerCommand)];
QueueHandle_t commands;

/** Short critical sections let other tasks read consistent recording progress. */
logger_status_t status;

StaticTask_t task_control;

/** FreeRTOS counts stack entries rather than bytes. This reserves 4 KiB. */
constexpr uint32_t LoggerStackDepth = 1024U;
StackType_t task_stack[LoggerStackDepth];

const SensorLog *find_sensor_file(sensor_source_t source)
{
    for (const auto &sensor : sensor_files) {
        if (sensor.source == source) {
            return &sensor;
        }
    }

    return nullptr;
}

FRESULT create_session_folder()
{
    FRESULT result = f_mkdir("0:/LOGS");
    if (result != FR_OK && result != FR_EXIST) {
        return result;
    }

    char candidate[LOGGER_PATH_SIZE];
    unsigned number = 1U;

    do {
        (void)std::snprintf(candidate, sizeof(candidate), "0:/LOGS/RUN%05u", number);
        result = f_mkdir(candidate);
        ++number;
    } while (result == FR_EXIST);

    if (result == FR_OK) {
        std::strcpy(session_path, candidate);
    }

    return result;
}

FRESULT open_recording()
{
    FRESULT result = create_session_folder();
    if (result != FR_OK) {
        return result;
    }

    for (const auto &sensor : sensor_files) {
        /** Leave room for a slash and a short filename such as GPS.CSV. */
        char path[LOGGER_PATH_SIZE + 13U];
        const int length = std::snprintf(path, sizeof(path), "%s/%s",
                                        session_path, sensor.filename);
        if (length < 0 || static_cast<size_t>(length) >= sizeof(path)) {
            return FR_INVALID_NAME;
        }

        result = sensor.file.open(path, sensor.header);
        if (result != FR_OK) {
            return result;
        }
    }

    return FR_OK;
}

FRESULT append_record(const log_record_t &record)
{
    const SensorLog *sensor = find_sensor_file(record.source);
    if (sensor == nullptr) {
        return FR_INVALID_PARAMETER;
    }

    char row[CSV_ROW_CAPACITY];
    if (sensor->format(&record, row, sizeof(row)) < 0) {
        return FR_INVALID_PARAMETER;
    }

    return sensor->file.append(row);
}

FRESULT close_files()
{
    FRESULT result = FR_OK;
    for (const auto &sensor : sensor_files) {
        /** Close every file even if one fails, and report the first error. */
        const FRESULT closed = sensor.file.close();
        if (result == FR_OK) {
            result = closed;
        }
    }

    return result;
}

bool can_accept_records()
{
    return commands != nullptr &&
        (status.state == LOGGER_STARTING || status.state == LOGGER_READY);
}

void finish(FRESULT result, bool stopped)
{
    taskENTER_CRITICAL();
    status.filesystem_error = static_cast<int>(result);
    status.state = result == FR_OK && stopped ? LOGGER_STOPPED : LOGGER_FAILED;
    taskEXIT_CRITICAL();

    /** New records are now rejected, so pending commands can be discarded. */
    if (commands != nullptr) {
        (void)xQueueReset(commands);
    }
}

FRESULT sync_log()
{
    FRESULT result = FR_OK;
    for (const auto &sensor : sensor_files) {
        if (sensor.file.is_open() && sensor.file.dirty()) {
            const FRESULT synced = sensor.file.sync();
            if (result == FR_OK) {
                result = synced;
            }
        }
    }

    if (result == FR_OK) {
        taskENTER_CRITICAL();
        status.synced_records = status.processed_records;
        taskEXIT_CRITICAL();
    }

    return result;
}

void logger_task(void *)
{
    FRESULT result = storage_mount();
    if (result == FR_OK) {
        result = open_recording();
    }

    taskENTER_CRITICAL();
    std::copy_n(session_path, sizeof(status.path), status.path);
    taskEXIT_CRITICAL();

    bool stopped = false;
    if (result == FR_OK) {
        taskENTER_CRITICAL();
        /** Preserve a stop that was requested while the card was mounting. */
        if (status.state == LOGGER_STARTING) {
            status.state = LOGGER_READY;
        }
        taskEXIT_CRITICAL();

        TickType_t last_sync = xTaskGetTickCount();
        TickType_t interval = pdMS_TO_TICKS(LOGGER_SYNC_INTERVAL_MS);
        if (interval == 0U) {
            interval = 1U;
        }

        while (result == FR_OK && !stopped) {
            /** Unsigned subtraction keeps the interval correct across tick wrap. */
            const TickType_t elapsed = xTaskGetTickCount() - last_sync;
            const TickType_t wait = elapsed < interval ? interval - elapsed : 0U;
            LoggerCommand command;
            if (xQueueReceive(commands, &command, wait) == pdPASS) {
                switch (command.kind) {
                case CommandKind::Record:
                    result = append_record(command.record);
                    if (result == FR_OK) {
                        taskENTER_CRITICAL();
                        ++status.processed_records;
                        taskEXIT_CRITICAL();
                    }
                    break;

                case CommandKind::Stop:
                    stopped = true;
                    break;
                }
            }

            /** Sync on schedule even when the queue never becomes empty. */
            if (result == FR_OK && !stopped &&
                static_cast<TickType_t>(xTaskGetTickCount() - last_sync) >= interval) {
                result = sync_log();
                last_sync = xTaskGetTickCount();
            }
        }
    }

    /** Startup can fail after opening a file. Always close files during cleanup. */
    const FRESULT closed = close_files();
    if (result == FR_OK) {
        result = closed;
    }

    if (result == FR_OK) {
        taskENTER_CRITICAL();
        status.synced_records = status.processed_records;
        taskEXIT_CRITICAL();
    }

    const FRESULT unmounted = storage_unmount();
    if (result == FR_OK) {
        result = unmounted;
    }

    finish(result, stopped);

    /** A recording ends here. Reboot to start again. */
    for (;;) {
        vTaskSuspend(nullptr);
    }
}

}

void logger_start_task(void)
{
    /** Startup runs before the scheduler, so no producer can race this setup. */
    if (status.state != LOGGER_NOT_STARTED) {
        return;
    }

    commands = xQueueCreateStatic(LOGGER_QUEUE_DEPTH, sizeof(LoggerCommand),
                                  queue_storage, &queue_control);
    status.state = LOGGER_STARTING;
    if (commands == nullptr || xTaskCreateStatic(logger_task, "sd_logger", LoggerStackDepth,
                                                nullptr, tskIDLE_PRIORITY + 1U,
                                                task_stack, &task_control) == nullptr) {
        finish(FR_INT_ERR, false);
    }
}

bool logger_submit(const log_record_t *record)
{
    if (record == nullptr || find_sensor_file(record->source) == nullptr) {
        taskENTER_CRITICAL();
        ++status.invalid_records;
        taskEXIT_CRITICAL();
        return false;
    }

    const LoggerCommand command{CommandKind::Record, *record};

    /** Keep submission and the state check together so a stop cannot split them. */
    taskENTER_CRITICAL();
    const bool accepted = can_accept_records() && xQueueSend(commands, &command, 0U) == pdPASS;
    if (accepted) {
        ++status.accepted_records;
    } else {
        ++status.dropped_records;
    }
    taskEXIT_CRITICAL();

    return accepted;
}

bool logger_request_stop(void)
{
    LoggerCommand command{};
    command.kind = CommandKind::Stop;

    taskENTER_CRITICAL();
    const bool accepted = can_accept_records() && xQueueSend(commands, &command, 0U) == pdPASS;
    if (accepted) {
        status.state = LOGGER_STOPPING;
    }
    taskEXIT_CRITICAL();

    return accepted;
}

void logger_get_status(logger_status_t *out)
{
    if (out == nullptr) {
        return;
    }

    taskENTER_CRITICAL();
    *out = status;
    out->queued_commands = commands == nullptr ? 0U :
        static_cast<uint32_t>(uxQueueMessagesWaiting(commands));
    taskEXIT_CRITICAL();
}
