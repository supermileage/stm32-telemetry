# UBC Supermileage Telemetry

Firmware for the STM32U585 on the B-U585I-IOT02A board. The goal is to collect
vehicle sensor data and save it to an SD card.

The main application mounts the card, checks file access, and starts a binary
logger. Sensor acquisition is still in development, so logs contain only a
header until a producer is connected. A PC log decoder and host test suite are
not currently included.

## Flash a release

1. Download the release `.bin`.
2. Connect the board's ST-LINK USB port (CN8) to your computer.
3. Drag the `.bin` onto the ST-LINK USB drive. The ST-LINK LED should flash;
   `FAIL.TXT` on the drive indicates a programming failure.

## Build and run

Clone the repository and initialize its library submodules:

```bash
git clone https://github.com/supermileage/stm32telemetry.git stm32-telemetry
cd stm32-telemetry
git submodule update --init --recursive
```

Import `firmware/` into STM32CubeIDE, select Debug or Release, and build.
The [firmware guide](firmware/README.md#build-and-flash) covers flashing and
project settings. Follow the [card check](docs/storage_logging.md#check-the-card)
to confirm SD access on the board.

## How it works

Sensor producers submit copied records to a queue. One storage task writes
them through FatFs and the SPI SD driver. It buffers writes, syncs periodically,
and rotates log files. The [architecture guide](docs/architecture.md) explains
the code boundaries; [storage and logging](docs/storage_logging.md) covers the
API, limits, and shutdown sequence.

## Where to find things

| Folder | Contents |
| --- | --- |
| [firmware/](firmware/README.md) | Main firmware and board support |
| `lib/` | [SD driver](lib/drivers/sd_spi/README.md), [FatFs](lib/fatfs/README.md), and other dependencies |
| `docs/` | [Architecture](docs/architecture.md), [hardware](docs/hardware.md), [storage and logging](docs/storage_logging.md), and [binary format](docs/logging_format.md) |
| [SubProjects/](SubProjects/README.md) | Separate board experiments |

Project plans are in the
[telemetry plan](https://app.notion.com/p/Vehicle-Telemetry-System-3ee7e02c955f806296e9d68eaa08e37a)
and [division plan](https://app.notion.com/p/Software-Embedded-Division-Project-Plan-2026-2027-3bb7e02c955f80a282bec8cfb8830d0f).

## Versioning and Releases

Version tags use `vMAJOR.MINOR.PATCH`: major for project milestones such as the
MVP, minor for new capabilities or breaking changes, and patch for fixes and
small backward-compatible changes.

Make a release when a feature works or the project reaches a useful milestone.
Start with a prerelease, then promote it after testing on the car. Record what
changed and what was tested. See the [release guide](release.md) for the steps.
