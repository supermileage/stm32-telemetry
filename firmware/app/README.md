# Application Code

This folder contains the application startup, storage, logging, and sensor code.

## Files

| Path | Purpose |
| --- | --- |
| `app.h`, `app.cpp` | Connect C startup code to the C++ application |
| `storage/` | SD card setup, mounting, and unmounting |
| `logging/` | Logger task, record queue, and binary log files |
| `sensors/` | Sensor data collection and encoding |

## Startup

`app_init()` runs after peripheral and RTOS setup, before the scheduler starts.

1. `app_init()` creates the logger task and queue using `logger_start_task()`.
2. Once the scheduler starts, the logger task mounts the SD card using `storage_mount()`.
3. The logger opens a log file and starts writing queued records.

**Only the logger task accesses FatFs and the SD driver.** Sensor tasks send records through the queue instead of writing files directly.

Use `logger_get_status()` to check startup progress or errors. Flush and stop requests also go through the queue and are processed in order.

See the [storage and logging guide](storage/storage_logging.md) for flushing and stopping the logger.

Storage and logging headers work with both C and C++.

## Sensor Logging

Sensor tasks encode samples and send them to the logger, which writes them to binary files with framing and CRC checks.

Source IDs are defined in `sensors/sources.h`.

See the [sensor guide](sensors/README.md) for submitting records and the [binary format](logging/logging_format.md) for reading log files.