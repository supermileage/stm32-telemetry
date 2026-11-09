# Board Support Package (BSP)

The BSP connects the application to the **B-U585I-IOT02A** board using STM32 HAL and FreeRTOS.

It provides hardware interfaces for peripherals such as SPI, UART, and the SD card, keeping board-specific code separate from the application.

## Structure

| Path | Purpose |
|---|---|
| `spi/` | SPI communication and bus management |
| `uart/uart_port.h`, `.c` | C UART state, interrupt RX and serialized TX |
| `storage/sd_spi_port.hpp`, `.cpp` | SD card SPI setup and bus access |
| `storage/sd_diskio.h`, `.cpp` | Connects the SD card driver to FatFs |
| `storage/sd_card.h` | C interface for SD card setup |

## Peripheral Access

Peripheral drivers handle hardware communication and synchronization using STM32 HAL and FreeRTOS.

For SPI, `spi_bus.c` provides transfers and mutex-based bus locking. Devices sharing a bus use separate chip-select pins, and transactions must be completed before releasing the bus.

SPI operations must run from FreeRTOS tasks, not interrupts. Avoid direct HAL SPI transfers on a bus managed by the BSP.

Peripheral settings such as clock speed, pin assignments, and operating modes are configured in STM32CubeMX.

## C peripheral driver contracts

CubeMX, HAL, FreeRTOS and peripheral drivers use C. Application tasks and protocol
processing may use C++ and include these headers directly through their
`extern "C"` declarations. No C++ wrapper is needed. The existing SD/FatFs C++
adapters are unchanged by this SPI/UART refactor.

Give each `spi_bus_t` and `uart_port_t` static/boot lifetime and zero-initialize it
(for example, `static uart_port_t sensor_uart;`). Never copy an initialized object
or modify its fields: HAL and FreeRTOS retain pointers into it. All users of a
peripheral must share its one BSP object and avoid direct HAL access.

The normal initialization order is:

1. CubeMX initializes GPIO, clocks and peripheral handles.
2. Kernel setup runs, then `app_init()` initializes BSP objects serially before
   the scheduler starts. UART init creates its static stream/mutex and publishes
   callback ownership; it does not enable reception.
3. Tasks are created and the scheduler starts.
4. The single UART reader task calls `uart_port_start_rx()`, then reads buffered
   bytes. TX tasks call `uart_port_write()`.

SPI also permits initialization from a task before first use. The existing SD
adapter uses this path from its sole logger task; retaining it preserves SD
startup behavior. Do not initialize that bus concurrently from another task.
There is no shutdown/rebinding lifecycle. Repeating init with the same handle
succeeds without recreating resources; binding a different handle fails.

### SPI ownership and cleanup

`spi_begin()` takes the static mutex and asserts CS low. `spi_transfer()` passes
its millisecond timeout directly to HAL and leaves CS/ownership intact on both
success and failure. Every successful begin **must** be paired with `spi_end()`
from the same task and with the same device, including on error paths:

```c
if (spi_begin(&device, pdMS_TO_TICKS(20))) {
    bool ok = spi_transfer(&device, tx, rx, sizeof(tx), 50U);
    spi_end(&device); /* Always raise CS and release the mutex. */
    /* Handle ok here. */
    (void)ok;
}
```

The mutex owner prevents other tasks from accessing the bus. `active_device`
also prevents the owning task from selecting or ending a different device on
that bus. Nested acquisition fails before taking the non-recursive mutex,
avoiding a wait on the calling task's own lock. `spi_end()` raises CS before
clearing ownership and giving the mutex; it works even after a HAL error.

`spi_acquire()` and `spi_select()` remain necessary for the SD adapter: it must
clock with CS high before commands and after deselection while keeping the mutex.
Other devices normally need only begin/transfer/end. SPI hardware format is
asserted once at init; mode and clock remain at their CubeMX settings.

### UART use and timing

There are no UART application callers yet. A C++ task can use the C BSP directly:

```cpp
#include "uart/uart_port.h"
#include "usart.h"

static uart_port_t sensor_uart;

// In app_init(), after CubeMX/kernel setup and before the scheduler:
// if (!uart_port_init(&sensor_uart, &huart3)) { /* handle startup failure */ }

// In the single reader task, after the scheduler starts:
// if (!uart_port_start_rx(&sensor_uart)) { /* inspect status / retry */ }
// uint8_t bytes[32];
// size_t count = uart_port_read(&sensor_uart, bytes, sizeof(bytes), pdMS_TO_TICKS(20));
```

One port owns each of `huart1`, `huart3` and `huart4` through a fixed callback
table. Duplicate claims fail during init. Unsupported/unclaimed handles are
ignored by the RX/error callbacks; TX and other peripheral callbacks are untouched.
The build requires `USE_HAL_UART_REGISTER_CALLBACKS == 0U`.

Init requires idle 8-bit/no-parity UARTs with FIFO disabled, matching the current
CubeMX settings and the reviewed single-byte HAL path. RX-only/TX-only modes
remain usable for their respective operations. The RX stream holds **256 bytes**;
its 257-byte backing array includes FreeRTOS's reserved ring slot. On the current
Cortex-M33 build, each `uart_port_t` occupies 404 bytes of static RAM; the shared
three-entry callback table adds 36 bytes. There is one
ISR writer and one task reader, as required by stream buffers. Full buffers drop
new bytes, increment `dropped_bytes` and keep reception armed. Reads can return
short chunks and do not identify protocol message boundaries.

TX uses blocking `HAL_UART_Transmit()` under its own static mutex. `timeout_ms`
is one shared budget: mutex waiting consumes part of the time and HAL receives
the remainder. If a nonzero budget expires during acquisition, no transmission
starts. Zero polls; `HAL_MAX_DELAY` waits indefinitely. Unsigned subtraction
handles a HAL tick wrap. Tick granularity and scheduling can delay the return;
this is not a hard real-time deadline. HAL failure may follow partial output,
so blind retries can duplicate bytes. Every acquired TX mutex is released.

The project uses a 1 kHz RTOS tick and 32-bit `TickType_t`, so milliseconds map
directly to ticks without multiplication overflow. A compile-time check requires
reviewing this conversion if either setting changes. Runtime APIs reject ISR,
pre-scheduler and suspended-scheduler calls before using task-only FreeRTOS APIs.
Call with interrupts enabled, outside critical sections.

### UART interrupts and error recovery

The source review used bundled **STM32U5 HAL 1.6.3** and **FreeRTOS 11.2.0**:

- `UART_RxISR_8BIT()` reads `RDR`, disables RX interrupts and sets `RxState` ready
  before invoking completion. Normal completion can safely call `Receive_IT()`.
- `HAL_UART_IRQHandler()` reads error flags from `ISR`, clears them through `ICR`,
  records `ErrorCode`, and can invoke RX completion **before** the error callback.
  Completion must leave `ErrorCode` intact and discard the suspect byte. Rearming
  clears `ErrorCode`; doing it early can hide overrun and break HAL recovery.
- Overrun/receiver-timeout ends RX before the error callback; rearm when HAL is
  ready. Noise/framing without a completed byte may leave RX busy; keep that
  transfer running. Failed rearming sets `active` false so the reader can retry.
- Completion uses `xStreamBufferSendFromISR()` and passes the accumulated wake
  flag to `portYIELD_FROM_ISR()`. Counters wrap modulo 2^32. A hardware error and
  its failed recovery count as one event; a standalone failed arm also counts.

`uart_port_start_rx()` checks NVIC enable/priority before its short critical
section. That section masks the UART IRQ while `Receive_IT()` arms hardware and
publishes `rx_active`; the HAL call does not wait. Status snapshots use another
short critical section to read all counters consistently. No blocking operation
runs in either section. `volatile` makes ISR updates visible; interrupt masking
provides synchronization.

For the key RX lines in `uart_port.c`:

```c
BaseType_t woken = pdFALSE;
if (xStreamBufferSendFromISR(port->rx_stream, &port->rx_byte, 1U, &woken) != 1U) {
    ++port->rx_dropped_bytes;
}
port->rx_active = HAL_UART_Receive_IT(handle, &port->rx_byte, 1U) == HAL_OK;
portYIELD_FROM_ISR(woken);
```

`woken` starts false so an ISR requests a context switch only when FreeRTOS
unblocks a higher-priority task. The send copies one byte into the stream; a
short send means the ring is full, so the counter records that loss. The HAL
call reuses the port's single-byte storage for the next interrupt and publishes
whether arming succeeded (the implementation also counts a failed arm).
The final line requests the task switch on ISR exit. This is interrupt RX, not
DMA: every received byte runs this path, so measure interrupt load on the board.

Current generated code enables **USART3 at priority 5**, with priority grouping 4
(all four implemented bits used for preemption). This meets
`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY == 5`: numerically smaller values
are more urgent and cannot call FreeRTOS. UART1/UART4 IRQs are currently disabled;
their TX works, but starting RX fails until CubeMX enables their IRQs and handlers
at a safe priority. Keep priorities/grouping fixed after startup. No hardware
configuration or generated code was changed.

### Verification and learning exercises

See [BSP host checks](../../tests/bsp/README.md) for repeatable ownership,
overflow, recovery and timeout regressions. Host mocks do not measure interrupt
latency or prove electrical behavior; verify those on the board:

1. Inspect CS/SCK with a logic analyzer while forcing a transfer timeout. Confirm
   the caller still calls `spi_end()` and the next task can acquire the bus.
2. Pause UART consumption, send more than 256 bytes, then inspect
   `uart_port_rx_status()`. Resume reading and confirm RX continues. At the
   configured baud rate, measure worst-case interrupt load and check for overrun.
3. Use a debugger to watch `RxState`, `ErrorCode`, `rx_active` and `rx_errors`
   through overrun recovery. Breakpoints can themselves cause overruns because
   the sender keeps transmitting. Prefer counters, trace or a logic analyzer for
   timing investigations. Compare task stack high-water marks under load.

Check your understanding: why must the SD adapter keep the mutex while CS is
high, and why must RX completion defer rearming when `ErrorCode` is nonzero?

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
