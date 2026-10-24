# Application Code

This folder contains the application startup, storage, logging, and sensor code.

Hardware setup stays in `Core/` and `BSP/`, while reusable drivers are in `lib/`.

## Files

| Path | Purpose |
| --- | --- |
| `app.h`, `app.cpp` | Connect C startup code to the C++ application |
| `storage/` | SD card setup and filesystem operations |
| `logging/` | Log queue, binary file writing, and file rotation |
| `sensors/` | Sensor data collection and encoding |

## Startup

`app_init()` runs after the RTOS and peripherals are initialized, but before the scheduler starts.

1. `storage_start_task()` creates the storage task and logger queue.
2. When the scheduler starts, the storage task calls `storage_mount()` to configure the SD card and mount the filesystem.
3. The storage task opens the log file and handles all SD card operations.

Storage errors are reported through logger status.

## Sensor Logging

Each sensor encodes its data before sending it to the logger. The logger queues the records and writes them to a binary file with framing and CRC checks.

Source IDs are defined in `sensors/sources.h`. A PC decoder for converting binary
logs to CSV is not included yet.

See [sensor payloads](sensors/README.md) for the IMU format and a logging example.
