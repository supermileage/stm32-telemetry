# Board Support

This folder connects the SD card driver to the STM32 hardware using HAL. It handles SPI communication and pin configuration. SD card commands are handled by the driver, while file operations and storage tasks are handled in `app/`.

## Files

| Files | Purpose |
| --- | --- |
| `sd_spi_port.h`, `.cpp` | Handles SPI transfers, chip select, clock speed, and driver callbacks |
| `b_u585i_iot02a/board_sd.h`, `.cpp` | Configures SPI1 (`hspi1`) and the PE12 chip select pin |

## SD Card Setup

Call `bsp::configureSd()` after CubeMX initializes SPI and GPIO.

This function:
- Sets chip select (CS) high
- Sets the initial SPI clock to 400 kHz or lower
- Connects STM32 HAL functions to the SD driver
- Configures timing using `HAL_GetTick()` and `HAL_Delay()`

This only sets up communication. It does not send commands to the SD card.

SPI uses mode 0, 8-bit transfers, and software-controlled chip select. SD card access should only happen in the storage task, not in interrupts.
