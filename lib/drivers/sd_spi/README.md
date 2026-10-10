# SD SPI driver

Maintained SD card protocol adapted from kiwih/cubeide-sd-card and ChaN.
The original source revision and copyright notice are retained in `sd_spi.c`.

| Location | Responsibility |
| --- | --- |
| `lib/drivers/sd_spi/sd_spi.c` / `.h` | Protocol, 512-byte block I/O, readiness, and diagnostics |
| `firmware/BSP/sd_spi_port.c` / `.h` | STM32U5 HAL transfers, clock dividers, CS, and timing |
| `firmware/BSP/b_u585i_iot02a/board_sd.c` | Board SPI1 handle and CS pin |
| `firmware/App/storage/sd_diskio.c` | FatFs adapter |

The driver compiles directly. No patch or `prepare.cmake` step is needed.
The old `lib/sd_card/upstream` submodule is retained as a reference and is
excluded from builds. Edit the maintained source to change the protocol.

## Use

The storage task owns all driver and FatFs calls. On this board,
`board_sd_configure()` calls `sd_spi_port_bind()`, then FatFs initializes the
card. For another platform, supply `sd_spi_port_t` callbacks to `sd_spi_bind()`;
the driver copies the callbacks and has no HAL dependency.

Use a dedicated bus: mode 0, 8-bit, MSB first, software CS. The U5 port keeps
SCK/MOSI driven between transfers and limits startup to 400 kHz and transfers
to 4 MHz (250 kHz / 4 MHz with this board's 16 MHz SPI clock).
Each byte uses a blocking HAL call; measure throughput under the intended load.

An I/O or protocol error clears readiness. Close files and unmount before
reinitializing. `sd_spi_is_ready()` reports cached state; there is no card detection.
`sd_spi_get_diagnostics()` returns lifetime failure counters and the last result.

SD transfer CRC checking is disabled; received CRCs are consumed and discarded.
Log frame CRCs do not protect FAT metadata or the SPI transfer.
See [wiring and shutdown](../../../firmware/App/storage/README.md),
[tests](../../../tests/sd_spi/README.md), and
[ChaN's protocol notes](https://elm-chan.org/docs/mmc/mmc_e.html).
