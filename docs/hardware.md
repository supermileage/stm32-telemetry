# Hardware

The firmware targets the B-U585I-IOT02A board.

## SD card

Format the card as FAT32 before use. FAT12/16 also work; exFAT and formatting
from firmware are disabled. Use 3.3 V logic, power the SD module as specified
by its manufacturer, and connect a common ground.

| SD signal | MCU pin | Firmware binding |
| --- | --- | --- |
| SCK | PA5 | SPI1 SCK |
| MISO | PB4 | SPI1 MISO |
| MOSI | PG4 | SPI1 MOSI |
| CS | PE12 | `SD_CS_Pin`, `SD_CS_GPIO_Port` |

SPI1 uses mode 0, 8-bit transfers, MSB first, and software CS. The current
16 MHz clock source gives 250 kHz during setup (/64) and 4 MHz after card
initialization (/4). [`board_sd.cpp`](../firmware/BSP/b_u585i_iot02a/board_sd.cpp)
binds SPI1 and CS to the driver.