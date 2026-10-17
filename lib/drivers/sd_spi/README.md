# SD SPI driver

Portable C driver for one card on a dedicated SPI bus. It supports SD v1/v2,
SDSC and block-addressed cards, and single- or multi-sector I/O. A legacy MMC v3
initialization fallback is also implemented. Transfers are blocking; the core
uses no HAL, RTOS, filesystem, or dynamic allocation.

[sd_spi.h](sd_spi.h) documents the public API and supports C and C++ callers.
The optional [FatFs adapter](fatfs/README.md) connects it to ST's disk dispatcher.

## Build

Compile `sd_spi.c` as C11 and add this directory to the include path, or use
the supplied CMake target:

```cmake
add_subdirectory(path/to/sd_spi)
target_link_libraries(application PRIVATE sd_spi)
```

This target builds only the protocol core. Compile `fatfs/sd_diskio.c` separately
if the application uses FatFs. The STM32 HAL port lives in the
[BSP](../../../firmware/BSP/README.md).

## Platform callbacks

Configure full-duplex, mode-0 SPI with 8-bit words, MSB first, and active-low
software CS. Bind these five callbacks with `sd_spi_bind()`:

| Callback | Contract |
| --- | --- |
| `exchange(tx, rx)` | Transfer one byte in both directions, including when CS is high. Use a finite timeout and return false on failure. |
| `set_speed(maximum_hz)` | Select a clock at or below the limit. Called with CS high; return false if configuration fails. |
| `chip_select(selected)` | Drive CS low for true and high for false. |
| `get_tick_ms()` | Return advancing milliseconds modulo 2^32, including during polling. |
| `delay_ms(milliseconds)` | Wait at least the requested duration; initialization requests 10 ms. |

All callbacks run in the caller's context. The driver copies the callback table,
but hardware and callback state must remain valid. There is no context pointer
or bus lock. Keep the timebase running; an interrupt-driven tick will stall if
interrupts are disabled.

The driver requests at most 400 kHz during initialization and 4 MHz afterward.
Slower clocks are allowed.

## Card access

1. Configure the hardware and leave CS high.
2. Call `sd_spi_bind()`. Binding resets readiness but sends no card commands.
3. Call `sd_spi_initialize()` from the owning task.
4. After `SD_OK`, query capacity and read, write, or sync.

Addresses are zero-based logical sectors of 512 bytes, including for SDSC cards.
Buffers must cover `count * 512` bytes for the whole call. Arithmetic overflow
is checked, but requests are not checked against card capacity; use
`sd_spi_get_sector_count()` to establish the valid range.

Keep all driver calls, including readiness and diagnostic queries, in one
owning context. Do not call from interrupts or callbacks that reenter the driver.
When FatFs owns the card, use FatFs for normal I/O and file syncs.

## Errors and recovery

| Result | Meaning |
| --- | --- |
| `SD_OK` | Completed successfully. |
| `SD_INVALID_ARGUMENT` | Invalid pointer, count, or address range. Invalid I/O arguments leave readiness unchanged; an invalid bind removes the binding. |
| `SD_NOT_READY` | Card is not initialized, or initialization was rejected, timed out, or could not set the clock. |
| `SD_ERROR` | Transfer, protocol, busy timeout, or capacity decoding failed. |

A failed card operation clears readiness. Later valid requests return
`SD_NOT_READY` until initialization succeeds. There are no automatic retries;
a failed multi-sector request may have completed some sectors without reporting
how many.

After a filesystem error, stop I/O, attempt to close files, and unmount before
rebinding or reinitializing. With ST's FatFs dispatcher, also unlink and
re-register the disk to clear its cached initialization state. Check cleanup
results before remounting. `sd_spi_sync()` waits for the card to become ready;
it does not flush application or filesystem buffers.

`sd_spi_get_diagnostics()` reports operation and callback failures, invalid
arguments, rejected requests, and the last result. Counters wrap at 32 bits and
persist across binding and initialization; one failure can affect several
counters. See the header for field definitions.

## Limits and validation

- Readiness is cached. There is no card detection, hot-plug handling, write-protect
  sensing, power control, erase/TRIM, or native SDMMC support.
- Sector addresses and capacity use `uint32_t`. Unsupported CSD layouts or a
  capacity that does not fit return `SD_ERROR`.
- SD data CRCs are not enabled or checked. CMD0 and CMD8 use their required command
  CRCs. The logger's record CRC does not protect FAT metadata.
- Busy polling, data-token waits, and initialization use phase timeouts of
  500 ms, 200 ms, and 1000 ms respectively. These are not overall call deadlines.

Validate wiring, clock timing, card compatibility, and sustained throughput on
hardware. Follow the [storage guide](../../../docs/storage_logging.md) for the
application's card check and shutdown sequence.

## Source and license

Derived from kiwih/cubeide-sd-card revision
`89261c6a4997232c68ff4f61cc9ecdd20c60e227`, based on ChaN's SPI card routines.
Copyright notices and redistribution terms are retained in [sd_spi.c](sd_spi.c).
The [original snapshot](../../sd_card/README.md) is kept for reference and is
excluded from builds.
