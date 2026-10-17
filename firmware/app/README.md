# Application Code

This folder contains the main application code, including startup, storage, logging, and sensor tasks. Hardware configuration stays in `Core/` and `BSP/`, while reusable drivers are kept in `lib/`.

## Files

| Path | Purpose |
| --- | --- |
| `app.h`, `app.cpp` | Connects C startup code to the C++ application |
| `storage/` | Handles SD card mounting, file checks, and the storage task |
| `logging/` | Manages the logging queue, record encoding, and file rotation |
| `sensors/` | Sensor data collection (work in progress) |

## Startup

The RTOS initialization calls `app_init()` after the peripherals and kernel are set up, but before the scheduler starts.

The startup process:

1. Configures the SD SPI interface using `bsp::configureSd()`.
2. Calls `Error_Handler()` if configuration fails.
3. Creates the storage task using `storage_start_task()`.
4. Once the scheduler starts, the storage task handles SD card and filesystem operations.