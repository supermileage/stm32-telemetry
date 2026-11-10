# Board support package

The BSP connects application code to the B-U585I-IOT02A board using STM32 HAL and
FreeRTOS. It handles SPI, UART, and the SD card. Peripheral pins and settings come
from STM32CubeMX.

| Folder or file | Purpose |
| --- | --- |
| `spi/` | SPI transfers and shared bus access |
| `uart/uart_port.h`, `.c` | Buffered UART reception and blocking transmission |
| `storage/sd_spi_port.h`, `.c` | SD card transfers over SPI |
| `storage/sd_diskio.h`, `.c` | Connect FatFs to the SD driver |
| `storage/sd_card.h` | SD card setup interface |

## Object lifetime and startup

Keep one static BSP object per peripheral. Initialize it once, keep using the
same HAL handle, and do not copy or change its fields directly.

```c
static uart_port_t sensor_uart;
```

CubeMX sets up the hardware first. `app_init()` prepares the UART driver after
kernel setup and before the scheduler starts. The reader task starts reception
after the scheduler is running. SPI setup can also run from a task; the logger
uses this when mounting the SD card.

The drivers are written in C. Their headers support C++ callers through
`extern "C"`. Use the BSP APIs for peripherals they manage, so their locks and
state stay consistent.

## SPI

A normal SPI transaction uses three calls:

1. `spi_begin()` locks the bus and selects the device.
2. `spi_transfer()` transfers bytes.
3. `spi_end()` deselects the device and releases the bus.

Always call `spi_end()` after a successful `spi_begin()`, including when a
transfer fails. The same task must begin and end the transaction on the same
device.

```c
if (spi_begin(&device, pdMS_TO_TICKS(20))) {
    bool ok = spi_transfer(&device, tx, rx, sizeof(tx), 50U);
    spi_end(&device);

    if (!ok) {
        // Handle the transfer error.
    }
}
```

`spi_begin()` waits for the mutex in FreeRTOS ticks. `spi_transfer()` uses
milliseconds for its transfer timeout. Use `pdMS_TO_TICKS()` when converting a
wait time.

The SD driver uses `spi_acquire()` and `spi_select()` because it sometimes needs
clock cycles with chip-select high. Other devices should use the normal three
calls. The BSP does not change the CubeMX SPI settings.

## UART

The UART driver supports USART1, USART3, and UART4. Initialize each port before the
scheduler starts:

```c
bool ok = uart_port_init(&sensor_uart, &huart3);
```

Initialization creates the driver's static RTOS objects. It does not start RX.
From the reader task, call `uart_port_start_rx()` and check its return value.

### Receive data

Only one task may read a port. Interrupts queue up to 256 received bytes, each
paired with a HAL tick. These entries use 2,048 bytes of storage plus queue control
data. A full queue drops new input and preserves what is already queued.

Use `uart_port_read()` for plain bytes. Use `uart_port_read_timed()` when reception
time matters, as it does for GPS:

```c
uart_rx_byte_t received[16];
size_t count = uart_port_read_timed(
    &sensor_uart, received, 16U, pdMS_TO_TICKS(20));
```

Capacity counts entries. Only the first entry can wait; later entries are read
without waiting. Reads may return fewer entries than requested. The driver returns
bytes, not assembled protocol messages.

Check `uart_port_rx_status()` for dropped bytes, errors, and active RX state. The
driver re-arms reception after each byte. If RX stops, the reader task can retry
`uart_port_start_rx()`.

The CubeMX UART setup must provide:

- 8-bit data, no parity, and FIFO disabled.
- An enabled UART IRQ and its handler.
- A FreeRTOS-safe IRQ priority: 5 or numerically higher with the current config.
- Priority grouping 4 and `USE_HAL_UART_REGISTER_CALLBACKS == 0U`.

### Send data

`uart_port_write()` locks the transmit mutex, sends the bytes, then releases it.
`wait_ticks` limits the mutex wait in RTOS ticks; `timeout_ms` limits HAL's transfer
in milliseconds. A failed transfer may have sent some bytes, so a blind retry can
duplicate output.

Call UART read, write, and RX-start functions from a running task with interrupts
enabled, outside a critical section.

## SD card

The SD card uses SPI1 at 250 kHz with these connections:

| Signal | STM32 pin |
| --- | --- |
| SCK | PE13 (Arduino D13) |
| MISO | PE14 (Arduino D12) |
| MOSI | PE15 (Arduino D11) |
| CS | PE12 |

Use 3.3 V logic, a common ground, and a FAT32-formatted card.

SPI1 uses the 16 MHz HSI kernel clock divided by 64, 8-bit mode 0, MSB first,
and software NSS. CS on PE12 (Arduino D10) is high before its output is enabled.
Both SD initialization and recording run at 250 kHz. See the
[board wiring guide](../config/board_wiring.md) for the other connector routes.

**Only the logger task accesses the SD card.** It calls `storage_mount()` to set
up the BSP and FatFs. Other tasks send records to the logger queue. See the
[logging guide](../app/storage/storage_logging.md) for sync and shutdown, and the
[library guide](../../lib/README.md) for third-party source files.

## Check and debug

See [host tests](../../tests/README.md) for UART buffering, timestamp, and recovery
checks. On the board, check SPI chip-select and lock release after errors, UART
overflow and restart behavior, and task stack headroom.

A logic analyzer is useful for timing checks. Breakpoints can stop reception and
cause overflows, so use status counters when checking continuous traffic.
