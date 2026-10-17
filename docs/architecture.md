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
| `firmware/app` | Application startup, tasks, and behavior |
| `firmware/app/logging` | Record queue, status, binary framing, buffering, and rotation |
| `firmware/app/storage` | Storage task, mount policy, and read-back test |
| `lib/drivers/sd_spi` | Portable SD protocol, block I/O, readiness, and diagnostics |
| `lib/drivers/sd_spi/fatfs` | Optional STM32Cube FatFs disk adapter and registration |
| `lib` | Reusable drivers and pinned dependencies; linked as `Lib` in CubeIDE |
| `firmware/BSP` | Board pins, peripheral handles, and hardware adapters |

Sensor sampling, payload encoding, and communication are planned for
`app/sensors`, `app/telemetry`, and `app/communication`. The current IMU experiment
is in [SubProjects/B-U5-RTOS-IMU](../SubProjects/B-U5-RTOS-IMU/README.md).
IMU sampling, wireless transport, and GNSS are not integrated into the main
application yet.

## Startup and logging

Generated `main.c` initializes HAL, clocks, peripherals, and the RTOS kernel.
`MX_FREERTOS_Init()` calls the C++ `app_init()`, which configures the board's SD
SPI port and binds the C protocol driver. Binding leaves CS inactive and sends
no card commands. The generated default task then runs after scheduler start.

The C storage/logging implementation is exercised by host tests but is not
started by the current application or included in the CMake firmware target.
When started, its storage task mounts the card, writes and reads back a Hello
World file, and opens a binary log in normal mode. Producers submit copied
records through `logger_submit()`; the task writes and periodically syncs them.
Its standalone Hello World mode unmounts after the check. The
`TELEMETRY_SD_HELLO_TEST` build option is not currently connected to startup.

## Storage ownership

Only the storage task accesses FatFs and the dedicated SPI1 bus. FatFs is
configured without locking between tasks, so producers must use the logger API.
The queue, task stack, and write buffer use static allocation. FreeRTOS uses
`heap_1`, which does not support task deletion; the storage task idles after
shutdown.

A full queue rejects new records. To shut down, stop producers, request a logger
stop, and wait for `LOGGER_STOPPED` before removing the card. See the
[logging API](../firmware/app/logging/README.md) for queue limits, sync behavior,
and error handling, and the [binary format](logging_format.md) for record encoding.

## Builds and regeneration

CMake uses explicit source lists. CubeIDE builds `Core`, `Drivers`, `Middlewares`,
`app`, `Lib`, and `BSP`, with exclusions for backups and unused vendor sources.

The SD driver in `lib/drivers/sd_spi` uses hardware callbacks from
`firmware/BSP/sd_spi_port.cpp`. Its optional `fatfs/sd_diskio.c` translates disk
requests without depending on application code or BSP. Host tests compile that
adapter explicitly; CubeIDE discovers it under the linked `Lib` source folder.
The original SD source is kept as a reference and excluded from builds.
CubeIDE builds the FatFs wrappers; CMake builds the upstream implementations
directly. Compiling both would duplicate symbols.

Keep application behavior in `app`, hardware adaptation in `BSP`, reusable
protocol and filesystem adapters in `lib`, and edits to generated files in
`USER CODE` sections. The `.ioc` enables user-code preservation. After CubeMX
regeneration, review both CubeIDE build configurations and keep CMake source
lists in sync. Commit portable `.settings` preferences with the project metadata.

See the [firmware guide](../firmware/README.md) for build instructions,
[host tests](../tests/README.md) for verification, and [hardware](hardware.md)
for wiring and datasheets.
