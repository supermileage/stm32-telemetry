# SD SPI block driver

Blocking SD/MMC SPI protocol driver with a portable C interface. Hardware access
is provided by five callbacks; the core has no STM32 HAL, RTOS, or filesystem
dependency and performs no dynamic allocation. The public header supports C
and C++ callers.

## Capabilities

- SD v1/v2 initialization with byte-addressed SDSC and block-addressed cards.
  A legacy MMC v3 initialization fallback is also implemented.
- Single-sector and multi-sector reads and writes using 512-byte logical sectors.
- Card busy polling, write-status checks, and CSD v1/v2 capacity decoding.
- Cached readiness and cumulative failure diagnostics.
- One card, one global driver instance, and one dedicated SPI bus.

The [API reference](sd_spi.h) documents each function's arguments, results, and
side effects. Filesystem access is available through the optional
[STM32Cube FatFs adapter](fatfs/README.md).

## Package layout and build

| File | Purpose |
| --- | --- |
| [sd_spi.h](sd_spi.h) | Public API, callback contract, results, and diagnostics |
| [sd_spi.c](sd_spi.c) | Card commands, addressing, transfers, and state |
| [CMakeLists.txt](CMakeLists.txt) | C11 static library target `sd_spi` |
| [fatfs/](fatfs/README.md) | Optional adapter for ST's FatFs disk dispatcher |

Compile `sd_spi.c` as C11 and add this directory to the include path. With CMake,
add the directory and link the `sd_spi` target:

```cmake
add_subdirectory(path/to/sd_spi)
target_link_libraries(application PRIVATE sd_spi)
```

The core target does not build the FatFs adapter. Projects that need filesystem
access compile `fatfs/sd_diskio.c` separately with their FatFs configuration.

## Platform port

Initialize SPI and the CS GPIO before calling `sd_spi_bind()`. Use full-duplex,
mode-0 SPI, 8-bit words, MSB first, and software-controlled active-low CS.

| Callback | Required behavior |
| --- | --- |
| `exchange(tx, rx)` | Complete one simultaneous transmit/receive byte; return false on failure. Provide a finite transfer timeout. Clock bytes even while CS is inactive, and leave CS control to the driver. |
| `set_speed(maximum_hz)` | Select an actual SPI clock at or below the requested limit. Called with CS inactive; return false if the limit cannot be met or configuration fails. |
| `chip_select(selected)` | Assert CS low for true and release it high for false. |
| `get_tick_ms()` | Return advancing milliseconds modulo 2^32, including during polling. Unsigned subtraction handles tick wraparound. |
| `delay_ms(milliseconds)` | Wait at least the requested duration; initialization requests 10 ms. |

All five callbacks are mandatory and run synchronously in the caller's context.
The callback table is copied, so it may be a local variable. Referenced hardware
and callback state must outlive the binding. There is no user-context pointer,
bus lock, bus-sharing protocol, or callback reentrancy support.

The driver requests a maximum of **400 kHz during initialization** and **4 MHz
after initialization**. These limits are fixed in `sd_spi.c`. Slower achievable
clocks are acceptable. Keep the timebase running and avoid calling the driver
with interrupts disabled if the port relies on an interrupt-driven tick.

## Initialization and block access

1. Configure the platform peripherals and leave CS inactive.
2. Call `sd_spi_bind()` with a complete callback table. Binding clears readiness
   and deasserts CS; it sends no card commands.
3. Call `sd_spi_initialize()` from the owning task or foreground context.
4. After `SD_OK`, query capacity and issue reads, writes, or syncs.

For example, a platform can expose a startup function using its callback table:

```c
#include "sd_spi.h"

sd_result_t card_start(const sd_spi_port_t *hardware)
{
    if (!sd_spi_bind(hardware)) {
        return SD_INVALID_ARGUMENT;
    }
    return sd_spi_initialize();
}

sd_result_t card_read_first_sector(uint8_t buffer[SD_SPI_SECTOR_SIZE])
{
    /* Call only after card_start() succeeds. */
    return sd_spi_read(0U, buffer, 1U);
}
```

Sector addresses are always zero-based logical block addresses, including for
SDSC cards; the driver performs byte-address conversion when required. Buffers
must cover `count * 512` bytes and remain valid for the duration of the call.
No DMA-specific alignment is required by the core. Address arithmetic and count
overflow are checked, but requests are not checked against physical capacity.
Use `sd_spi_get_sector_count()` to establish the valid range.

## Ownership and recovery

Serialize **all** driver calls, including readiness and diagnostics, in one
owning context. The driver is not reentrant and provides no internal locking.
Do not call it from interrupts or callbacks that reenter the driver. If FatFs
owns the card, route normal I/O through FatFs and use its sync/close operations
to flush file data and metadata.

| Result | Meaning and effect |
| --- | --- |
| `SD_OK` | Operation completed successfully. |
| `SD_INVALID_ARGUMENT` | Invalid pointer, zero count, or unrepresentable address/byte range. Read/write/capacity argument rejection leaves readiness unchanged. An invalid bind removes the binding. |
| `SD_NOT_READY` | Card is not initialized. During initialization, also covers card rejection, initialization timeout, and failure to set a requested clock. |
| `SD_ERROR` | Byte transfer, protocol, busy timeout, or capacity decoding failed. The failed card operation clears readiness. |

An unsuccessful initialization leaves readiness false. A failed read, write,
sync, or capacity query clears readiness; subsequent valid operations return
`SD_NOT_READY` until initialization succeeds. No automatic retry occurs. A
failed multi-sector operation may have completed part of the request; the API
does not report how many sectors completed.

After a filesystem error, stop new I/O, attempt to close open files, and unmount
before reinitializing or rebinding. With the ST adapter, unlink and re-register
the disk to clear the dispatcher's cached initialization state. Inspect cleanup
errors before remounting. `sd_spi_sync()` only waits for card readiness; it does
not flush filesystem or application buffers.

## Diagnostics

`sd_spi_get_diagnostics()` copies operation failure counts, byte-transfer and
speed callback failure counts, invalid arguments, rejected not-ready requests,
and `last_result`. Counters start at zero and persist across initialization and
rebinding; there is no reset API. One event can increment both a callback counter
and an operation counter. Counters wrap modulo 2^32.

`last_result` changes on card operations and invalid binding attempts. Successful
binding, `sd_spi_is_ready()`, and the diagnostics getter leave it unchanged.
Initialization failures have their own counter; `not_ready_requests` counts
read/write/sync/capacity requests rejected before reaching the card.

## Limits

- Readiness is cached. Card detection, write-protect sensing, power control,
  hot-plug management, erase/TRIM, and native SDMMC mode are not provided.
- Sector addresses and capacity use `uint32_t`. Capacity must fit in a 32-bit
  sector count; unsupported CSD layouts or larger counts return `SD_ERROR`.
- Data CRC bytes are transmitted as dummy values or discarded on reception.
  The driver does not enable or verify SD data CRCs; CMD0/CMD8 use their required
  command CRCs. Application checksums do not protect filesystem metadata.
- Transfers and polling are blocking. Busy waits use 500 ms, data-token waits
  use 200 ms, and the initialization loop uses a 1000 ms phase timer. These are
  phase limits, not an overall call deadline; callback latency and repeated
  phases also contribute to execution time.

## STM32U5 integration and verification

The [BSP](../../../firmware/BSP/README.md) supplies the STM32U5 HAL port and the
B-U585I-IOT02A SPI1/CS selection. The platform adapter stays outside this library.
Application mount policy, diagnostics, task setup, and logging remain in
[app/storage](../../../firmware/app/storage/README.md) and
[app/logging](../../../firmware/app/logging/README.md).

[Host tests](../../../tests/sd_spi/README.md) exercise SDHC, SD v2 SDSC, legacy
SDSC, transfers, callback failures, timeouts, and real FatFs on a simulated card.
They do not establish electrical timing, card compatibility, or throughput on
hardware. The legacy MMC fallback is not covered by the card model.

## Source and license

The protocol implementation derives from kiwih/cubeide-sd-card revision
`89261c6a4997232c68ff4f61cc9ecdd20c60e227`, based on ChaN's SPI card routines.
The copyright notices and redistribution terms are retained in [sd_spi.c](sd_spi.c).
The original snapshot is retained under [lib/sd_card](../../sd_card/README.md)
for reference and is excluded from builds.
