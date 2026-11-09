# Board Support Package (BSP)

The BSP connects the application to the **B-U585I-IOT02A** board using STM32 HAL and FreeRTOS.

It provides hardware interfaces for peripherals such as SPI, UART, and the SD card, keeping board-specific code separate from the application.

## Structure

| Path | Purpose |
|---|---|
| `spi/` | SPI communication and bus management |
| `storage/sd_spi_port.hpp`, `.cpp` | SD card SPI setup and bus access |
| `storage/sd_diskio.h`, `.cpp` | Connects the SD card driver to FatFs |
| `storage/sd_card.h` | C interface for SD card setup |

## Peripheral Access

Peripheral drivers handle hardware communication and synchronization using STM32 HAL and FreeRTOS.

For SPI, `spi_bus.c` provides transfers and mutex-based bus locking. Devices sharing a bus use separate chip-select pins, and transactions must be completed before releasing the bus.

SPI operations must run from FreeRTOS tasks, not interrupts. Avoid direct HAL SPI transfers on a bus managed by the BSP.

Peripheral settings such as clock speed, pin assignments, and operating modes are configured in STM32CubeMX.

## SD Card

The SD card uses **SPI1** with the following connections:

| Signal | STM32 Pin |
|---|---|
| SCK | PA5 |
| MISO | PB4 |
| MOSI | PG4 |
| CS | PE12 |

Use 3.3 V logic, a common ground, and a FAT32-formatted SD card.

The logger task initializes and mounts the card through `storage_mount()`. The BSP handles SPI setup, SD driver callbacks, and FatFs disk registration.

SPI1 is configured at 250 kHz and is dedicated to the SD card.

**Only the logger task should access the SD card.** Sensor tasks send records through the logger instead of writing files directly.

SD protocol handling lives in `lib/sd_spi`, storage setup in `app/storage/`, and file writing in `app/logging/`.

See the [library guide](../../lib/README.md) for the required SD driver sources.