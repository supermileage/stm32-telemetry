# Libraries

Shared drivers and third-party dependencies used by the firmware and host tests.

| Folder | Purpose |
| --- | --- |
| [drivers/sd_spi/](drivers/sd_spi/README.md) | Portable SD SPI block driver and optional STM32Cube FatFs adapter |
| [fatfs/](fatfs/README.md) | FatFs filesystem, configuration, and CubeIDE wrappers |
| [sd_card/](sd_card/README.md) | Original SD driver source, kept as a reference and excluded from builds |

The `upstream/` folders are pinned Git submodules. Initialize them as described
in [repository setup](../README.md#getting-started).

The SD protocol and filesystem-to-block-driver translation belong in
`drivers/sd_spi/`. Its core depends only on the C standard headers; its optional
`fatfs/` adapter depends on the SD API and ST's FatFs disk dispatcher.

STM32-specific SPI, chip select, and timing code belongs in
[`firmware/BSP`](../firmware/BSP/README.md). Application task setup, mount and
self-test policy, telemetry records, queues, log framing, and file rotation
belong in [`firmware/app`](../firmware/app/README.md).

CubeIDE shows this folder as `Lib`. It builds the FatFs wrappers; CMake builds
the upstream FatFs sources directly. Keep `fatfs/ffconf.h` matched to the pinned
FatFs version. See [host tests](../tests/README.md) to check driver changes.
