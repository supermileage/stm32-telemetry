# Application code

This folder is for the main application code: startup, logging, and eventually
the sensor and communication tasks.

Right now, `app.cpp` just sets up the SD card's SPI interface. The generated
RTOS code calls `app_init()` before the scheduler starts. `app.h` lets that C
code call the C++17 implementation.

`app_init()` calls `bsp::configureSd()` and goes to `Error_Handler()` if setup
fails. This doesn't send any commands to the card. After that, the generated
default task runs. Storage, sensor readings, and communication still need to
be connected to startup.

The other folders here are:

- [storage/](storage/README.md): mounting the card, checking a test file, and
  running the task that handles card access.
- [logging/](logging/README.md): queuing records, buffering writes, syncing,
  and starting a new file when the current one gets too large.

Those modules are still written in C and have host tests, but they aren't
started by `app_init()` or included in the CMake firmware target yet. The
[C++ plan](../../docs/sd_logging_cpp_implementation.md) covers the next steps.
So far, the startup and BSP stages are done. See the
[startup guide](../../docs/cpp_milestone_1.md) for the C/C++ build setup.

Keep application logic here, CubeMX setup in `Core`, and board pins and HAL
calls in [BSP](../BSP/README.md). The
[SD library](../../lib/drivers/sd_spi/README.md) handles the card protocol and
FatFs disk adapter. Constructors should just set up fields; call an explicit
setup function when hardware needs to be initialized.

The [API reference guide](../../docs/doxygen.md) explains how to generate and
check the Doxygen pages for this folder and the BSP. Keep call requirements
in the headers and use source comments to explain choices that need context.
