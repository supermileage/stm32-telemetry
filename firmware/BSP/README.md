# Board Support Package (BSP)

The BSP handles hardware communication for the **B-U585I-IOT02A** board using STM32 HAL and FreeRTOS.

It provides a common way to access peripherals such as SPI and UART, keeping hardware-specific code separate from the application.

## Structure

| Path | Description |
|---|---|
| `spi/spi_bus.h`, `.c` | SPI communication and bus management |
| `storage/sd_spi_port.hpp`, `.cpp` | Connects the SD card driver to SPI |
| `b_u585i_iot02a/board_sd.h`, `.cpp` | SD card pin assignments and board setup |
| `uart/` | UART communication |

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

**Note:** SPI1 is already configured correctly. SPI2 currently uses 4-bit data and hardware NSS, so its CubeMX settings need to be updated before using this module.

### Example

The following shows how to communicate with a SPI sensor.

**This is an example only.** In the current firmware, SPI1 is dedicated to the SD card and should not be shared with another device.

```c
#include "spi/spi_bus.h"
#include "spi.h"

static spi_bus_t bus = {0};

static spi_device_t sensor = {
    .bus = &bus,
    .cs_port = GPIOE,
    .cs_pin = GPIO_PIN_11,
};

// Initialize once after CubeMX peripheral setup.
bool sensor_bus_init(void)
{
    return spi_bus_init(&bus, &hspi1);
}

// Call from a FreeRTOS task.
bool sensor_exchange(
    const uint8_t *tx,
    uint8_t *rx,
    uint16_t size)
{
    if (!spi_acquire(&sensor, pdMS_TO_TICKS(50U))) {
        return false;
    }

    bool ok = spi_select(&sensor, true);

    if (ok) {
        ok = spi_exchange(&sensor, tx, rx, size, 50U);
    }

    spi_release(&sensor);
    return ok;
}
```

### How it works

| Function | Purpose |
|---|---|
| `spi_bus_init()` | Checks the CubeMX setup and creates the bus mutex |
| `spi_acquire()` | Locks the bus without changing its settings |
| `spi_select()` | Controls the device's chip-select pin |
| `spi_exchange()` | Sends and receives data |
| `spi_release()` | Deselects the device and unlocks the bus |

A device must acquire the bus before communicating and release it afterward, even if a transfer fails.

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

The storage task calls `storage_mount()`, which uses `board_sd_configure()` to set up SPI and register the SD driver callbacks.

Configure a fixed clock of 400 kHz or less in CubeMX. The SD driver uses that
same clock for initialization and normal reads and writes.

SPI1's `.ioc` configuration disables NSS pulses and keeps SCK/MOSI driven between
transfers. The generated `Core/Src/spi.c` uses those same settings. The current
16 MHz source and /64 prescaler give 250 kHz for all SD operations. There is no
clock lookup, divider selection, or speed switch in the application or driver.
Keeping the startup clock also means lower throughput than running at 4 MHz.

### Important notes

- SPI1 is dedicated to the SD card and must not be shared with other devices.
- All SD card operations run through a single storage task.
- Close all files and unmount the card before reconfiguring it.
- The SD adapter manages bus locking separately from chip select because SD initialization requires clock transfers even when CS is high.

SD card commands belong in the SD driver, while file operations and storage management belong in `app/storage/`.
