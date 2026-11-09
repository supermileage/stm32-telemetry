# SD Storage and Logging

The logger task handles writing sensor data to the SD card using FatFs and SPI1. Sensor tasks send records through a queue instead of accessing the SD card directly.

## Starting the Logger

`app_init()` calls `logger_start_task()` to create the logger task and queue. Once the scheduler starts, the logger mounts the SD card and opens a binary log file.

The logger uses statically allocated memory:

- A queue for 64 commands (4,864 bytes on the current ARM build)
- A 4 KiB task stack
- A 4 KiB write buffer

The task, queue, and status are managed in `logging/logger.c`. **Only the logger task accesses the filesystem.**

Storage has two jobs: `storage_mount()` prepares the card and mounts drive `0:/`; `storage_unmount()` detaches FatFs and unlinks the SD driver. The logger handles records, buffering, syncing, and closing the log file. There is no separate storage task.

Log files are named `LOGxxxxx.BIN`. The logger uses the first available filename and creates a new file before reaching 64 MiB. Existing files are never overwritten.

Calling `logger_start_task()` again has no effect, even after failure or shutdown.

## Checking the SD Card

Make sure the SD card is formatted as FAT32 and connected correctly. The logger configures and mounts the card using `storage_mount()`.

Mounting happens in this order:

1. `sd_card_configure()` checks the existing SPI setup and binds the SD driver callbacks.
2. `sd_diskio_register()` connects the driver to FatFs as drive `0:/`.
3. `f_mount(..., 1U)` initializes the card and reads the filesystem immediately.

Calling `storage_mount()` again while mounted returns `FR_OK` without repeating setup. If mounting fails, storage attempts to detach FatFs and unlink the driver. It keeps track of any unfinished cleanup so the logger can attempt it again before entering `LOGGER_FAILED`. The original mount error is preserved.

Use `logger_get_status()` to check startup progress and errors. You can also inspect the private `status` variable in `logger.c` using the STM32CubeIDE debugger.

| Field | Expected value |
|---|---|
| `state` | `LOGGER_READY` when the log is open |
| `filesystem_error` | `FR_OK` (0) if no error occurred |
| `path` | Current log filename |

If startup fails, check the SD card formatting, wiring, and SPI configuration. You can also inspect `result` after `storage_mount()` or `log_file_open()` in `logger_task()`.

## Logging Sensor Data

Sensor tasks submit records using `logger_submit()`.

Each record contains:

- **Timestamp:** Milliseconds since boot
- **Source ID:** Identifies the sensor or data format
- **Length:** Number of payload bytes
- **Payload:** Up to 64 bytes of encoded sensor data

Zero-initialize a `log_record_t`, fill its fields, and submit it using `logger_submit(&record)`.

Define sample types and payload encoders in the sensor module, and source IDs in `sensors/sources.h`. See the [sensor guide](../sensors/README.md) for details.

The logger stores payloads without interpreting them. The queue copies each record, so sensor tasks can reuse their memory immediately.

`logger_submit()` returns:

- `true` if the record was queued
- `false` if the logger is unavailable, the record is invalid, or the queue is full

**The function does not wait for queue space.** If the queue is full, the new record is dropped.

Wait until `LOGGER_READY` before submitting records when possible. Records queued during startup may be lost if initialization fails.

### Logging from Interrupts

Use `logger_submit_from_isr()` when submitting records from an interrupt:

```c
BaseType_t woken = pdFALSE;

bool accepted = logger_submit_from_isr(&record, &woken);

portYIELD_FROM_ISR(woken);
```

The interrupt priority must satisfy `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` in `FreeRTOSConfig.h`. The interrupt only queues the record; all SD operations run in the logger task.

Storage and logger headers work with both C and C++.

## Saving Data and Shutting Down

The logger buffers writes and syncs data to the SD card approximately once per second. Actual timing depends on SD card performance and task scheduling.

Use `logger_request_flush()` to request an additional sync. A `true` return means the request was queued; check `synced_records` for completion. Retry later if the queue is full.

To check whether submitted records have been saved, compare `synced_records` with an earlier `accepted_records` count. Both counters can wrap around.

### Stopping the Logger

Before removing the SD card or powering off:

1. Stop submitting sensor records.
2. Call `logger_request_stop()`. Retry later if the queue is full.
3. Wait until the state reaches `LOGGER_STOPPED`.

The logger finishes queued records, closes the log file, and unmounts the SD card.

The order matters: `log_file_close()` writes the remaining RAM buffer and closes the file before `storage_unmount()` detaches the filesystem. Calling `storage_unmount()` directly from a sensor task would bypass this shutdown sequence.

If startup, writing, or cleanup fails, the logger enters `LOGGER_FAILED`.

After cleanup, the task suspends without periodic wakeups. **Reboot the board to restart logging.**

## Monitoring the Logger

Use `logger_get_status()` from a task to check logger state and performance.

| Field | Meaning |
|---|---|
| `accepted_records` | Records added to the queue |
| `processed_records` | Records passed to the file writer |
| `synced_records` | Records saved by a successful sync or file close |
| `dropped_records` | Valid records rejected because they could not be queued |

`queued_commands` includes pending records, flush requests, and stop requests. It does not include a command the logger is already processing. `invalid_records` counts invalid records or ISR wake pointers.

`dropped_records` counts valid records rejected at submission. Records accepted before an I/O failure are not added to it; compare `accepted_records` with `synced_records` to see how far saving progressed. `filesystem_error` reports the first startup, I/O, or shutdown error, and `path` contains the last published log filename.

All record counters are 32-bit and can wrap around.

**Note:** A successful sync reduces the risk of data loss but does not guarantee protection against sudden power loss or filesystem corruption.

## Troubleshooting

| Error | What to check |
|---|---|
| `FR_NOT_READY` | SD card connection, power, and SPI setup |
| `FR_NOT_ENABLED` | SD driver registration; drive 0 must be available |
| `FR_NO_FILESYSTEM` | FAT32 formatting; exFAT is not supported |
| `FR_DISK_ERR` | SPI transfers, wiring, and SD driver |
| `FR_INT_ERR` during unmount | FatFs detach or SD driver unlinking failed |
| `LOGGER_FAILED` | Logger status and FatFs error codes |

If the card fills up or a write fails, logging stops. Fix the issue and reboot the board.

For a mount failure, step through `storage_mount()` and check which of the three setup calls failed. In `storage.c`, `registered` means the driver is linked to FatFs; `mounted` means mounting completed successfully. A failed mount can leave `registered` true until cleanup succeeds.

## Testing on Hardware

- **Queue:** Check queue usage and `dropped_records` at the highest sensor rate, including SD initialization, sync, and file rotation delays.
- **Stack:** Use `uxTaskGetStackHighWaterMark()` to measure stack headroom during startup, continuous logging, and failure cleanup.
- **Sync:** Submit records continuously and verify that `synced_records` increases even when the queue is busy. Sync timing is not a hard deadline.
- **Shutdown:** Fill the queue and request a stop. A rejected request must allow logging to continue. An accepted request must finish earlier records and reject new ones.
