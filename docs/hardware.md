# Hardware

The main firmware targets the B-U585I-IOT02A board and STM32U585AIIxQ.
The current pin configuration is in
[`firmware/stm32_telemetry.ioc`](../firmware/stm32_telemetry.ioc); generated
pin labels are in [`main.h`](../firmware/Core/Inc/main.h).

## SD card

Use a FAT12/16/32-formatted card; FAT32 is recommended for the bring-up test.
exFAT and formatting are disabled in the current FatFs configuration. The
module must use 3.3 V compatible signals, suitable power, and common ground.

| SD signal | MCU pin | Firmware binding |
| --- | --- | --- |
| SCK | PA5 | SPI1 SCK |
| MISO | PB4 | SPI1 MISO |
| MOSI | PG4 | SPI1 MOSI |
| CS | PE12 | `SD_CS_Pin`, `SD_CS_GPIO_Port` |

SPI1 is dedicated to the storage task. The driver uses mode 0, 8-bit,
MSB-first transfers and software chip select. With the configured 16 MHz
SPI1 kernel clock it initializes at 250 kHz, then transfers at 4 MHz.
The driver derives dividers from the actual clock if that configuration changes.
`BSP/b_u585i_iot02a/board_sd.cpp` binds `hspi1` and the CS pin to the SD driver.

See [storage diagnostics](../firmware/app/storage/README.md) for the file check
and the remaining startup integration work.

## IMU and planned interfaces

The independent `SubProjects/B-U5-RTOS-IMU` experiment configures the onboard
ISM330DHCX through I2C2 at 7-bit address `0x6B`. Main firmware sampling and a
reusable driver are not integrated yet. See its
[register notes](../SubProjects/B-U5-RTOS-IMU/README.md).

No GNSS receiver or wireless telemetry transport is integrated. Configure
their wiring and peripheral bindings before adding communication tasks.

## Programming

Connect the board's ST-LINK USB port (CN8). Import `firmware` into CubeIDE
for build, flash, and debug. CMake binaries are programmed at `0x08000000`
through STM32CubeProgrammer. See [firmware setup](../firmware/README.md).

Reference manuals and datasheets are retained in [`docs/datasheets`](datasheets).
