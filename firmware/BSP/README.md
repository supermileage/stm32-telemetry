# Board support

The SD driver in `lib` handles the card commands. The code here connects
that driver to the board's SPI bus and GPIO pins through STM32 HAL.

- `sd_spi_port.h` and `.cpp` handle byte transfers, chip select (CS), and SPI
  speed. The header works from C and C++; `bsp::SdSpiPort` is the C++ part.
- `b_u585i_iot02a/board_sd.h` and `.cpp` choose SPI1 (`hspi1`) and PE12 for
  CS. C code calls `board_sd_configure()`, and C++ code can call
  `bsp::configureSd()`. Both do the same setup.

## How it works

After CubeMX sets up GPIO and SPI, `app_init()` calls `bsp::configureSd()`.
This leaves CS high and sets the SPI clock to 400 kHz or less. It also gives
the SD driver the functions it needs to use the hardware, including
`HAL_GetTick()` and `HAL_Delay()` for timing.

This only sets up the interface. It doesn't send commands to the card yet.
Card initialization still needs to be called from a task after the scheduler
starts. If setup fails, previously registered callbacks are removed and
`app_init()` calls `Error_Handler()`.

`sd_spi_port_bind()` keeps one static port object, so it stays around while
the driver uses it. The driver copies the table of callback functions.
Constructing the port object doesn't touch the hardware; setup happens when
it's configured.

SPI uses mode 0, 8-bit transfers, MSB first, separate send/receive lines, and
software CS. Each byte transfer has a 50 ms HAL timeout. The port picks the
fastest clock divider that stays within the requested speed, using the actual
SPI clock. CS must be high before changing speed. NSS pulses are disabled,
and SCK/MOSI stay driven between transfers.

With the current 16 MHz clock, setup uses /64 (250 kHz). The driver requests
4 MHz once the card is initialized. The pins are SCK PA5, MISO PB4, MOSI PG4,
and CS PE12.

Use the functions in `sd_spi.h` to initialize the card, read and write sectors,
sync, and get its capacity. Keep all card access in one storage task, outside
interrupts. Close files and unmount the card before setting up the port again
or reinitializing the card.

The [FatFs disk adapter](../../lib/drivers/sd_spi/fatfs/README.md) lives with
the driver in `lib`. Mounting the card and running the storage task belong in
[app/storage](../app/storage/README.md).

## Testing

Run the [host tests](../../tests/README.md) after changing this code.
`sd_spi_port_cpp` checks setup, clock limits, CS, transfers, timer wraparound,
and failed setup attempts using the real SD driver. The SD/FatFs test also
uses this BSP with the storage code and log writer.

On the board, put breakpoints at `app_init()` and `bsp::configureSd()`. PE12
should be high and SPI1 should use /64 after setup. Once card initialization
is connected, check SCK and CS with a logic analyzer too; the host tests can't
check the actual signals. If CubeIDE still has build objects from the old C
BSP files, refresh the project and do a clean build.

The [API reference guide](../../docs/doxygen.md) covers the Doxygen build and
documentation checks for both the BSP and application headers.
