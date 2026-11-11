# Board support package

The BSP provides SPI, UART, and SD card drivers for the B-U585I-IOT02A, using
STM32 HAL and FreeRTOS. Configure pins and peripherals in STM32CubeMX.

| Folder or file | Purpose |
| --- | --- |
| `spi/` | SPI transfers and shared bus access |
| `uart/uart_port.*` | Buffered reception and blocking transmission |
| `storage/sd_spi_port.*` | SD card transfers over SPI |
| `storage/sd_diskio.*` | Connect FatFs to the SD driver |
| `storage/sd_card.h` | SD card setup interface |

## Setup

Use one static BSP object per peripheral. Initialize it once with the same HAL
handle. Do not copy it or change its fields directly.

```c
static uart_port_t sensor_uart;
```

CubeMX sets up the hardware first. `app_init()` initializes the UART driver
before the scheduler starts; the reader task starts reception later. SPI setup
can run from a task, as the logger does when mounting the card.

The drivers are C, with headers that support C++ callers. Access peripherals
through the BSP so its locks and state stay valid.
