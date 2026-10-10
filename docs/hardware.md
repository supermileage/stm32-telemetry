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
`BSP/b_u585i_iot02a/board_sd.c` binds `hspi1` and the CS pin to the SD driver.

See [the physical card test](sd_hello_test.md) and
[storage diagnostics](../firmware/App/storage/README.md).

## IMU and planned interfaces

The independent `SubProjects/imu_test` experiment accesses the onboard
ISM330DHCX through I2C2 at 7-bit address `0x6B`. Main firmware sampling and a
reusable driver are not integrated yet. See its
[register notes](../SubProjects/imu_test/README.md).

No GNSS receiver or wireless telemetry transport is integrated. Configure
their wiring and peripheral bindings before adding communication tasks.

## Programming

Connect the board's ST-LINK USB port (CN8). Import `firmware` into CubeIDE
for build, flash, and debug. CMake binaries are programmed at `0x08000000`
through STM32CubeProgrammer. See [firmware setup](../firmware/README.md).

Reference manuals and datasheets are retained in [`docs/datasheets`](datasheets).
