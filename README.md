# UBC Supermileage Telemetry

Firmware for UBC Supermileage's STM32U585 telemetry system. The goal is to
collect vehicle sensor data and save it to an SD card.

The current firmware configures the SD card's SPI interface and starts the
FreeRTOS scheduler. The C storage and logging modules have host tests, but
are not connected to application startup or included in the CMake firmware
target yet. Sensor acquisition is still being developed.

## Quick start (flash a release)
1. Download release (.bin)
2. Connect to the board to your computer via microUSB (STLK CN8 connector)
3. The board will show up as a USB drive, drag the .bin into it. ST-LINK LED should flash, if `FAIL.TXT` is created then it failed

## Build and run

Clone the repository and initialize its library submodules:

```bash
git clone https://github.com/supermileage/stm32telemetry.git stm32-telemetry
cd stm32-telemetry
git submodule update --init --recursive
```

For the B-U585I-IOT02A board, install CMake 3.25+, Ninja, and the GNU Arm
toolchain. Put the Arm tools on PATH, or set `ARM_TOOLCHAIN_BIN` to their
`bin` directory. From the repository root:

```bash
cd firmware
cmake --preset debug
cmake --build --preset debug
```

The output is `build/debug/stm32_telemetry.elf`, `.hex`, and `.bin` at the
repository root. Use the `release` preset for an optimized build. Connect
through the board's ST-LINK USB port (CN8) and use STM32CubeProgrammer to
write the `.bin` at `0x08000000`, then reset the board. The
[firmware guide](firmware/README.md) also covers building and flashing in CubeIDE.

To run the host tests, install native GCC (`gcc` and `g++`) and run from
`firmware/`:

```bash
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests
```

## How it works

The C++ BSP connects the portable C SD driver to STM32 SPI and GPIO. The
storage task, once connected to startup, mounts the card through FatFs,
checks a text file, and opens a binary log. Producers copy records into a
64-command queue; the task buffers writes, syncs periodically, and rotates
files at 64 MiB. A full queue rejects the newest submission. See the
[logging guide](firmware/app/logging/README.md) for the API and shutdown sequence.

| Folder | Contents |
| --- | --- |
| [firmware/](firmware/README.md) | Main firmware and board support |
| [lib/](lib/README.md) | Shared drivers and dependencies |
| [tests/](tests/README.md) | Host tests with simulated hardware |
| [docs/](docs/architecture.md) | Architecture, hardware, and log format |
| [SubProjects/](SubProjects/README.md) | Separate board experiments |

Project plans are in the
[telemetry plan](https://app.notion.com/p/Vehicle-Telemetry-System-3ee7e02c955f806296e9d68eaa08e37a)
and [division plan](https://app.notion.com/p/Software-Embedded-Division-Project-Plan-2026-2027-3bb7e02c955f80a282bec8cfb8830d0f).
See [release.md](release.md) for the release process.

## Versioning and updates
Semantic versioning will be used: `vMAJOR.MINOR.PATCH`
- Major will be incremented when a milestone is hit such as MVP
- Minor will be incremented for new new features/capabilities or breaking change
- Patch will be incremented for bug fixes and minor backward compatible changes

When a new feature like a sensor has been impemented and works well a release should be made. Other signficant achievements should also be released, use judgment. Detail changes (features added or removed, things broken/not backward compatible) in description so it can act as a changelog. \
Any release should begin as a pre-release, if it has been tested on the car and works well it should become a release, a note on what is functioning would also be good.

To see how to make a release please view [release tutorial](release.md)