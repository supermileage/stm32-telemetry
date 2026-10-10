# Libraries

Shared drivers and third-party dependencies used by the firmware and host tests.

| Folder | Purpose |
| --- | --- |
| [drivers/sd_spi/](drivers/sd_spi/README.md) | Maintained SD card protocol driver |
| [fatfs/](fatfs/README.md) | FatFs filesystem, configuration, and CubeIDE wrappers |
| [sd_card/](sd_card/README.md) | Original SD driver source, kept as a reference and excluded from builds |

The `upstream/` folders are pinned Git submodules. Initialize them as described
in [repository setup](../README.md#getting-started).

Edit `drivers/sd_spi/` to change the SD protocol. STM32-specific SPI, chip select,
and timing code belongs in `firmware/BSP/sd_spi_port.c`.

CubeIDE shows this folder as `Lib`. It builds the FatFs wrappers; CMake builds
the upstream FatFs sources directly. Keep `fatfs/ffconf.h` matched to the pinned
FatFs version. See [host tests](../tests/README.md) to check driver changes.
