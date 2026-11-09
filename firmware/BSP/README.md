# Board Support Package (BSP)

The BSP handles hardware communication for the **B-U585I-IOT02A** board using STM32 HAL and FreeRTOS.

It provides blocking SPI transactions and connects the SD protocol library to
SPI1 and FatFs.

## Structure

| Path | Description |
|---|---|
| `spi/spi_bus.h`, `.c` | SPI communication and bus management |
| `storage/sd_spi_port.hpp`, `.cpp` | SD card setup, SPI callbacks, and bus locking |
| `storage/sd_diskio.h`, `.cpp` | Connects the external SD library to FatFs |
| `storage/sd_card.h` | C entry point for SD setup, implemented in `sd_spi_port.cpp` |

## SPI

The SPI module provides blocking transfers and allows multiple devices to share the same SPI peripheral safely.

Each SPI peripheral has one `spi_bus_t` instance and one mutex. All devices on
that peripheral use the same bus object, with a separate chip-select pin per
device. Separate peripherals have separate mutexes and can run independently.

### Setup

Before using a SPI device:

1. Configure SPI mode, clock, and GPIO using STM32CubeMX. Set all CS outputs high.
2. Create a `spi_bus_t` for the peripheral.
3. Define each device's bus and chip-select pin.
4. Initialize the bus once before tasks begin using it.

The SPI peripheral must use master mode, full-duplex 8-bit transfers, MSB-first, and software chip select, with TI mode and CRC disabled.

The wrapper leaves these settings alone. Devices on a bus normally use the same
mode and clock configured in CubeMX. Acquiring the bus does not call `HAL_SPI_Init()`.

`spi_bus_init()` returns false for invalid handles or a peripheral that is not
ready. Fixed configuration mistakes trigger separate `configASSERT()` checks,
so the debugger stops at the setting that needs fixing. Update the CubeMX
configuration and regenerate if one of these assertions fires.

SPI2 uses 4-bit data and hardware NSS. Update those CubeMX settings before
using this module with SPI2.

### How it works

| Function | Purpose |
|---|---|
| `spi_bus_init()` | Checks the CubeMX setup and creates the bus mutex |
| `spi_begin()` | Locks the bus and drives the device's CS low |
| `spi_transfer()` | Sends and receives bytes, keeping CS unchanged |
| `spi_end()` | Drives CS high and unlocks the bus, even after transfer failure |

A device must begin a transaction before communicating and end it afterward,
even if a transfer fails:

```c
if (spi_begin(&device, pdMS_TO_TICKS(50))) {
    bool ok = spi_transfer(&device, tx, rx, size, 50);
    spi_end(&device);
    /* Handle ok here; always end the transaction before returning. */
}
```

Keep the lock across the whole transaction, including separate command and data
transfers. Another task must not select its device in the middle of that sequence.

Configure the mode and clock in CubeMX. They stay fixed for every transaction.
Devices sharing a bus must support that same mode and clock. There is no runtime
configuration API in the wrapper.

### Important notes

- SPI operations must run from FreeRTOS tasks, not interrupts.
- Only one task can own a bus at a time. Do not acquire the same bus again while holding it.
- Do not use direct HAL SPI transfers on a bus managed by this module.
- Multiple transfers can be performed during one acquisition.
- Both TX and RX buffers are required. Use dummy bytes such as `0xFF` when receiving.
- Bus locking uses FreeRTOS ticks, while transfer timeouts use milliseconds.
- Bus objects must remain valid and must not be copied after initialization.
- The module uses static synchronization objects and does not allocate memory dynamically.

The SD adapter retains two compatibility operations: `spi_acquire()` locks with
CS high, and `spi_select()` changes CS while keeping the lock. It uses
`spi_transfer()` and `spi_end()` for transfers and cleanup. These operations are
needed for SD's CS-high clocks; ordinary devices use the four-function API above.

## SD Card

The SD card uses **SPI1**, with the following connections:

| Signal | STM32 Pin |
|---|---|
| SCK | PA5 |
| MISO | PB4 |
| MOSI | PG4 |
| CS | PE12 |

Use 3.3 V logic, a common ground, and a FAT32-formatted SD card.

### Initialization

The storage task calls `storage_mount()`, which uses `sd_card_configure()` to
check SPI1 and bind the SD library callbacks. `sd_diskio_register()` connects
the FatFs adapter as drive 0, and `f_mount()` initializes the card.

The SPI1 and chip-select setup lives in `sd_spi_port.cpp`.
`sd_card.h` exposes setup to C application code, while
`sd_spi_port.hpp` is the internal C++ interface used by the FatFs adapter.

Configure a fixed clock of 400 kHz or less in CubeMX. The SD driver uses that
same clock for initialization and normal reads and writes.

SPI1's `.ioc` configuration disables NSS pulses and keeps SCK/MOSI driven between
transfers. The generated `Core/Src/spi.c` uses those same settings. The current
16 MHz source and /64 prescaler give 250 kHz for all SD operations. Board setup
reads the selected peripheral clock and divider to validate this rate; it does
not change the peripheral settings. Both library speed modes use this fixed clock.
Keeping the startup clock limits throughput; this port does not switch to a
faster transfer clock.

### Important notes

- SPI1 is dedicated to the SD card and must not be shared with other devices.
- All SD card operations run through a single storage task.
- Close all files and unmount the card before reconfiguring it.
- The SD adapter manages bus locking separately from chip select because SD initialization requires clock transfers even when CS is high.

SD card commands belong in the SD driver, while file operations and storage management belong in `app/storage/`.

The SD protocol code lives in the `lib/sd_spi` submodule. See the
[library guide](../../lib/README.md) for source selection and validation.
