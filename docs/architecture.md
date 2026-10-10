# Firmware architecture

The firmware separates generated STM32 setup, application behavior, and reusable
drivers. `firmware/` is the STM32CubeIDE project root; `lib/` holds shared code
and dependencies, and `tests/` holds tests that run on a PC.

## Where code belongs

| Directory | Responsibility |
| --- | --- |
| `firmware/Core` | CubeMX-generated peripheral setup, startup, interrupts, RTOS setup |
| `firmware/Drivers` | STM32 HAL and CMSIS |
| `firmware/Middlewares` | FreeRTOS and CMSIS RTOS integration |
| `firmware/App` | Application startup, tasks, and behavior |
| `firmware/App/logging` | Record queue, status, binary framing, buffering, and rotation |
| `firmware/App/storage` | Storage task, FatFs disk adapter, mount and read-back test |
| `lib` | Reusable drivers and pinned dependencies; linked as `Lib` in CubeIDE |
| `firmware/BSP` | Board pins, peripheral handles, and hardware adapters |

Sensor sampling, payload encoding, and communication are planned for
`App/sensors`, `App/telemetry`, and `App/communication`. The current IMU experiment
is in [SubProjects/B-U5-RTOS-IMU](../SubProjects/B-U5-RTOS-IMU/README.md).
IMU sampling, wireless transport, and GNSS are not integrated into the main
application yet.

## Startup and logging

1. Generated `main.c` initializes HAL, clocks, peripherals, and the RTOS kernel.
2. `MX_FREERTOS_Init()` calls `app_init()`, which creates the storage task.
3. After the scheduler starts, the storage task mounts the card and writes and
   reads back a Hello World file. Normal mode then opens a new binary log.
4. Producers submit records through `logger_submit()`. The queue copies each
   record, and the storage task writes and periodically syncs them.

`TELEMETRY_SD_HELLO_TEST=1` selects the standalone file test. It unmounts the card
after the check instead of starting logging.

## Storage ownership

Only the storage task accesses FatFs and the dedicated SPI1 bus. FatFs is
configured without locking between tasks, so producers must use the logger API.
The queue, task stack, and write buffer use static allocation. FreeRTOS uses
`heap_1`, which does not support task deletion; the storage task idles after
shutdown.

A full queue rejects new records. To shut down, stop producers, request a logger
stop, and wait for `LOGGER_STOPPED` before removing the card. See the
[logging API](../firmware/App/logging/README.md) for queue limits, sync behavior,
and error handling, and the [binary format](logging_format.md) for record encoding.

## Builds and regeneration

CMake uses explicit source lists. CubeIDE builds `Core`, `Drivers`, `Middlewares`,
`App`, `Lib`, and `BSP`, with exclusions for backups and unused vendor sources.

The SD driver in `lib/drivers/sd_spi` uses hardware callbacks from
`firmware/BSP/sd_spi_port.c`. The original vendor driver is kept as a reference
and excluded from builds. CubeIDE builds the FatFs wrappers; CMake builds the
upstream implementations directly. Compiling both would duplicate symbols.

Keep application behavior in `App` and `BSP`, and edits to generated files in
`USER CODE` sections. The `.ioc` enables user-code preservation. After CubeMX
regeneration, review both CubeIDE build configurations and keep CMake source
lists in sync. Commit portable `.settings` preferences with the project metadata.

See the [firmware guide](../firmware/README.md) for build instructions,
[host tests](../tests/README.md) for verification, and [hardware](hardware.md)
for wiring and datasheets.
