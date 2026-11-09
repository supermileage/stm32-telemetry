# Board Support Package (BSP)

The Board Support Package (BSP) connects the application to the **B-U585I-IOT02A** board using STM32 HAL and FreeRTOS.

It handles communication with hardware peripherals such as SPI, UART, and the SD card. This keeps hardware-specific code separate from the application and provides a consistent way to access peripherals.

## Project Structure

| Path | Purpose |
|---|---|
| `spi/` | SPI communication and bus management |
| `uart/uart_port.h`, `.c` | UART communication, interrupt-based RX, and synchronized TX |
| `storage/sd_spi_port.hpp`, `.cpp` | SD card SPI communication |
| `storage/sd_diskio.h`, `.cpp` | Connects the SD card driver to FatFs |
| `storage/sd_card.h` | C interface for SD card initialization |

## Using the BSP

The BSP uses **STM32 HAL** for hardware communication and **FreeRTOS** for synchronization between tasks.

Peripheral settings such as GPIO pins, clock speeds, and operating modes are configured in STM32CubeMX.

### Initialization

The normal startup order is:

1. **CubeMX** initializes clocks, GPIO, and peripheral handles.
2. **FreeRTOS kernel setup** runs.
3. **`app_init()`** initializes BSP peripherals before the scheduler starts.
4. **FreeRTOS tasks** start using the peripherals.

SPI can also be initialized from a task before its first use. For example, the logger task initializes the SD card's SPI bus when setting up storage.

Each peripheral should have one shared BSP object with a lifetime that lasts for the entire application.

```c
static uart_port_t sensor_uart;
```

Keep these objects static, initialize them only once, and don't copy or modify them directly after initialization.

BSP peripheral drivers are written in C, but they can be used directly from C++ tasks. Their headers already support `extern "C"`, so no additional wrapper is needed.

**Important rules:**
- Use the BSP functions instead of calling HAL directly on a peripheral managed by the BSP.
- Don't call task-only BSP functions from interrupts.
- Don't initialize the same peripheral concurrently from multiple tasks.
- Initialize each peripheral with its intended HAL handle. Reinitializing with the same handle is safe, but switching handles is not supported.

## SPI Communication

The SPI driver in `spi/` manages data transfers and access to shared SPI buses.

Multiple SPI devices can share a bus as long as each has its own chip-select (CS) pin.

### SPI Transactions

Use three functions for normal SPI communication:

1. `spi_begin()` — Locks the bus and pulls the device's CS pin low.
2. `spi_transfer()` — Sends and receives data.
3. `spi_end()` — Raises CS and releases the bus.

Example:

```c
if (spi_begin(&device, pdMS_TO_TICKS(20))) {
    bool ok = spi_transfer(&device, tx, rx, sizeof(tx), 50U);

    // Always release the bus, even if the transfer fails.
    spi_end(&device);

    if (!ok) {
        // Handle transfer error.
    }
}
```

**Always call `spi_end()` after a successful `spi_begin()`, even when a transfer fails.**

The SPI driver uses a FreeRTOS mutex to prevent tasks from accessing the same bus at the same time. A transaction must begin and end in the same task, using the same device.

The timeout values have different units:
- `spi_begin()` uses FreeRTOS ticks for waiting on the mutex.
- `spi_transfer()` uses milliseconds for the HAL transfer timeout.

Use `pdMS_TO_TICKS()` when converting milliseconds to FreeRTOS ticks.

### Shared Bus Access

The SD card driver also uses `spi_acquire()` and `spi_select()`.

Unlike a normal SPI transaction, an SD card sometimes needs extra clock cycles while CS is high. These functions allow the driver to keep the bus locked while controlling CS separately.

Other SPI devices should normally use `spi_begin()`, `spi_transfer()`, and `spi_end()`.

SPI settings, including clock speed and mode, come from STM32CubeMX. The BSP does not change them during transactions.

## UART Communication

The UART driver in `uart/` provides interrupt-based reception (RX) and blocking transmission (TX).

It supports **USART1, USART3, and UART4**, with one BSP object per peripheral.

### Initialization

Initialize the UART port after CubeMX and kernel setup, before starting the scheduler.

```cpp
#include "uart/uart_port.h"
#include "usart.h"

static uart_port_t sensor_uart;

// Inside app_init():
bool ok = uart_port_init(&sensor_uart, &huart3);

if (!ok) {
    // Handle initialization failure.
}
```

Initialization prepares the UART driver and its FreeRTOS resources, but does not start receiving data.

### Receiving Data

UART reception uses interrupts to collect incoming bytes in a **256-byte buffer**.

Start reception from the task responsible for reading UART data, after the scheduler has started.

```cpp
if (!uart_port_start_rx(&sensor_uart)) {
    // Handle RX startup failure.
}

uint8_t buffer[32];

size_t count = uart_port_read(
    &sensor_uart,
    buffer,
    sizeof(buffer),
    pdMS_TO_TICKS(20)
);
```

The driver automatically re-arms UART reception after each received byte and handles UART error recovery.

Keep the following in mind:

- Only **one task should read** from a UART port.
- Reads can return fewer bytes than requested.
- The driver provides raw bytes, not complete protocol messages.
- If the receive buffer fills up, new bytes are dropped rather than overwriting existing data.
- Use `uart_port_rx_status()` to check dropped bytes and receive errors.
- If reception becomes inactive, the reader task can call `uart_port_start_rx()` again.

UART RX must have its interrupt enabled in STM32CubeMX, including its IRQ handler.

Because the interrupt uses FreeRTOS functions, its priority must be FreeRTOS-safe. With the current configuration, use **priority 5 or a numerically higher value**, with priority grouping 4.

The driver expects an 8-bit, no-parity UART with FIFO disabled. These settings are configured in CubeMX.

The build also requires `USE_HAL_UART_REGISTER_CALLBACKS == 0U`.

### Sending Data

Use `uart_port_write()` to send data.

```cpp
bool sent = uart_port_write(
    &sensor_uart,
    buffer,
    count,
    pdMS_TO_TICKS(20),
    50U
);
```

The UART driver uses a separate FreeRTOS mutex to prevent multiple tasks from transmitting at the same time.

There are two timeouts:

- `wait_ticks` — How long to wait for access to the UART transmitter.
- `timeout_ms` — How long HAL can spend transmitting after access is granted.

In the example, the task waits up to 20 ms for the mutex, then allows up to 50 ms for transmission.

A failed transmission may still have sent some bytes. Avoid blindly retrying if doing so could duplicate data.

UART read, write, and reception-start functions must be called from running FreeRTOS tasks, not from interrupts or critical sections.

## SD Card

The SD card uses **SPI1** to communicate with the STM32.

### Wiring

| Signal | STM32 Pin |
|---|---|
| SCK | PA5 |
| MISO | PB4 |
| MOSI | PG4 |
| CS | PE12 |

The SD card requires:
- 3.3 V logic
- A common ground with the STM32
- A FAT32-formatted SD card

SPI1 is configured to run at **250 kHz** and is dedicated to the SD card.

### Storage Access

The logger task initializes and mounts the SD card using `storage_mount()`.

The BSP handles SPI communication and connects the SD card driver to FatFs.

**Only the logger task should access the SD card.**

Other tasks, such as sensor tasks, should send their data to the logger rather than accessing files directly. This keeps file operations in one place and avoids concurrent storage access.

The SD card implementation is split into:

| Location | Responsibility |
|---|---|
| `lib/sd_spi/` | SD card SPI protocol |
| `app/storage/` | Storage initialization and mounting |
| `app/logging/` | Writing log data to files |

See the [library guide](../../lib/README.md) for SD card driver dependencies.

## Testing and Debugging

The BSP includes host-side tests for SPI ownership, UART buffering, error recovery, and timeouts.

See [BSP host checks](../../tests/bsp/README.md) for instructions.

Host tests verify driver behavior, but hardware timing and electrical communication still need to be checked on the board.

When testing on hardware:

1. **SPI:** Verify chip-select behavior and confirm the bus is released after a transfer error.
2. **UART RX:** Send more than 256 bytes without reading and check the dropped-byte counter. Confirm reception continues afterward.
3. **UART errors:** Check that reception recovers from overrun and other UART errors.
4. **FreeRTOS:** Check interrupt load and task stack usage under normal and heavy traffic.

A logic analyzer is useful for checking SPI signals and UART timing. Debugger breakpoints can interfere with interrupt-driven reception, so prefer counters or trace tools when investigating timing issues.
