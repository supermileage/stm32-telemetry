# SD Storage and Logging

The storage task handles writing sensor data to the SD card using FatFs and SPI1. Sensor tasks send their data to a queue, and the storage task takes care of saving it.

This keeps SD card operations separate from sensor collection.

## Starting the storage task

There are two ways to start storage:

| Function | What it does |
|---|---|
| `storage_start_task()` | Mounts the SD card and opens the binary log; no file self-test |
| `storage_start_hello_test()` | Tests reading and writing a text file without starting the logger |

By default, `app_init()` calls `storage_start_task()`.

To test only the SD card, replace it with `storage_start_hello_test()`.

Only one mode can run per boot. Reboot the board to restart storage after it stops or fails.

The storage task uses:
- A queue that holds up to 64 commands
- A 4 KiB task stack
- A 4 KiB write buffer

Log files are named `LOGxxxxx.BIN`, starting with the first available number. A new file is created before the current one exceeds 64 MiB. Existing logs are never overwritten.

## Checking the SD card

Make sure the SD card is formatted as FAT32 and connected correctly.

Normal startup mounts the card and opens the binary log. The storage task owns
SD interface configuration through `storage_mount()`; `app_init()` only creates
the task. Only diagnostic mode creates and reads back a text file.

You can check the results in the STM32CubeIDE debugger:

| Variable | Expected value |
|---|---|
| `storage_bringup_state` | `STORAGE_READY` or `STORAGE_TEST_PASSED` in test mode |
| `storage_mount_result` | `FR_OK` (0) |
| `storage_verify_result` | `FR_NOT_READY` in normal mode; `FR_OK` (0) after a successful diagnostic check |
| `storage_unmount_result` | `FR_OK` after shutdown |
| `storage_test_path` | Empty in normal mode; `0:/HELLO000.TXT` or the next available filename in diagnostic mode |

If mounting or verification fails, check the SD card formatting, wiring, and SPI configuration.

## Logging sensor data

Sensor tasks submit records using `logger_submit()`.

Each record contains:
- **Timestamp:** Time in milliseconds since boot
- **Source ID:** Identifies the sensor or data format
- **Length:** Number of payload bytes
- **Payload:** Up to 64 bytes of sensor data

Define sample types and payload encoders in the sensor module, and register
source IDs in `sensors/sources.h`. For example, submit an IMU vector from a C task:

```c
#include "logging/logger.h"
#include "sensors/imu_record.h"

bool log_imu_sample(const imu_sample_t *sample, uint32_t timestamp_ms)
{
    log_record_t record;
    return imu_record_encode(&record, timestamp_ms, sample) &&
        logger_submit(&record);
}
```

The [sensor payload guide](../sensors/README.md) documents the
units and byte layout. The generic logger does not interpret payloads.

The queue copies the record, so the sensor task can reuse its memory immediately.

`logger_submit()` returns:
- `true` if the record was added to the queue
- `false` if the logger is unavailable, the record is invalid, or the queue is full

The function does not wait for space. If the queue is full, the new record is dropped.

Ideally, wait until the logger reaches `LOGGER_READY` before submitting data. Records queued during startup can be lost if SD card initialization fails.

If submitting data from an interrupt, use `logger_submit_from_isr()` instead:

```c
BaseType_t woken = pdFALSE;

bool accepted = logger_submit_from_isr(&record, &woken);

portYIELD_FROM_ISR(woken);
```

The interrupt must use a FreeRTOS-safe priority (currently numerical priority 5 or greater).

For C++ code, wrap the storage and logger headers in `extern "C"`.

## Saving data and shutting down

The logger buffers data and syncs it to the SD card approximately once per second. Actual timing depends on SD card performance and task scheduling.

You can also request a sync using `logger_request_flush()`.

To confirm that submitted records have been saved, check `synced_records` against the earlier `accepted_records` count. Keep in mind that these counters can wrap around.

### Stopping the logger

Before removing the SD card or powering off the board:

1. Stop sending new sensor records.
2. Call `logger_request_stop()`. If the queue is full, retry later.
3. Wait until the logger reaches `LOGGER_STOPPED`.

When stopping, the logger finishes processing queued records, closes the log file, and unmounts the SD card.

If something goes wrong during startup, writing, or shutdown, the logger enters `LOGGER_FAILED`.

The storage task remains idle after stopping. Reboot the board to start it again.

## Monitoring the logger

Use `logger_get_status()` from a task to check the logger's state and performance.

| Field | Meaning |
|---|---|
| `accepted_records` | Records successfully added to the queue |
| `processed_records` | Records passed to the file writer |
| `synced_records` | Records covered by a successful SD card sync or file close |
| `dropped_records` | Valid records rejected because they could not be queued |

The status also includes the logger state, queue usage, invalid submissions, FatFs errors, and current log filename.

All record counters are 32-bit and can wrap around.

**Note:** A successful sync reduces the risk of data loss but cannot guarantee that data will survive sudden power loss or filesystem corruption.

## Troubleshooting

| Error | What to check |
|---|---|
| `FR_NOT_READY` | SD card connection, SPI configuration, and power |
| `FR_NO_FILESYSTEM` | SD card formatting; exFAT is not supported |
| `FR_DISK_ERR` | SPI transfers, wiring, and SD driver errors |
| `LOGGER_FAILED` | Logger status and FatFs error codes |

If the card becomes full or a write fails, logging stops. Fix the problem and reboot the board.

There is also a known issue in `log_file_close()` where the file may not close correctly after an `f_close()` failure. Reboot after a failed close.
