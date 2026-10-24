# UBC Supermileage Telemetry

This repository contains the firmware and supporting tools for the UBC Supermileage vehicle telemetry system.

The system uses an STM32U585 on the B-U585I-IOT02A development board to collect, process, and log vehicle data. The project is built with **STM32CubeIDE**, **STM32 HAL**, and **FreeRTOS**.

## Build and Run

Install Git and STM32CubeIDE, then clone the repository and initialize its submodules:

```bash
git clone https://github.com/supermileage/stm32telemetry.git stm32-telemetry
cd stm32-telemetry
git submodule update --init --recursive
```

Open STM32CubeIDE, import `firmware/`, select Debug or Release, and build the project.

See the [firmware guide](firmware/README.md#build-and-flash) for detailed build and flashing instructions.

## Flash a Release

1. Download the release `.bin` file.
2. Connect the board to your computer using the ST-LINK USB port (CN8).
3. Drag the `.bin` file onto the ST-LINK USB drive.

The ST-LINK LED should flash during programming. If `FAIL.TXT` appears on the drive, programming failed.

## Project Structure

| Folder | Description |
|---|---|
| [firmware/](firmware/README.md) | Main STM32 firmware |
| `lib/` | Shared drivers, libraries, and dependencies |
| `docs/` | Datasheets and reference documents |
| [tests/](tests/README.md) | Tests for firmware components |
| [SubProjects/](SubProjects/README.md) | Separate experiments and development projects |

For more details on the project, see the [telemetry plan](https://app.notion.com/p/Vehicle-Telemetry-System-3ee7e02c955f806296e9d68eaa08e37a) and [division plan](https://app.notion.com/p/Software-Embedded-Division-Project-Plan-2026-2027-3bb7e02c955f80a282bec8cfb8830d0f).

## Releases

Version tags use `vMAJOR.MINOR.PATCH`: major for project milestones such as the
MVP, minor for new capabilities or breaking changes, and patch for fixes and
small changes that keep existing behavior compatible.

Make a release when a feature works or the project reaches a useful milestone.
Start with a prerelease, then promote it after testing on the car. Record what
changed and what was tested. See the [release guide](release.md) for the steps.