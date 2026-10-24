# Board Support

This folder contains board-specific code for the B-U585I-IOT02A.

It connects the STM32 hardware to reusable drivers using HAL. Hardware details like pin assignments and peripheral connections stay here instead of in the application or drivers.

## Files

| Path | Purpose |
| --- | --- |
| `sd_spi_port.h`, `.cpp` | Connects the SD card driver to STM32 HAL |
| `b_u585i_iot02a/` | Board-specific hardware setup and pin assignments |

## SD Card

The SD card uses SPI1 with the following connections:

| SD Signal | MCU Pin |
| --- | --- |
| SCK | PA5 |
| MISO | PB4 |
| MOSI | PG4 |
| CS | PE12 |

Use 3.3 V logic and a common ground. Format the SD card as FAT32 before use.

The storage task calls `board_sd_configure()` to connect SPI1 and the chip select pin to the SD driver.

The BSP handles the hardware connection. The SD driver handles card commands, and `app/storage/` handles file operations.