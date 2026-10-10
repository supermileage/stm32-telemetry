# UBC Supermileage Telemetry

UBC Supermileage's vehicle telemetry system uses an STM32U585 to collect
vehicle sensor data and save it to an SD card.

Project plans and timelines are in the
[telemetry plan](https://app.notion.com/p/Vehicle-Telemetry-System-3ee7e02c955f806296e9d68eaa08e37a)
and [division plan](https://app.notion.com/p/Software-Embedded-Division-Project-Plan-2026-2027-3bb7e02c955f80a282bec8cfb8830d0f).

## Getting started

Install Git, then clone the repository and initialize its library submodules:

```bash
git clone https://github.com/supermileage/stm32telemetry.git stm32-telemetry
cd stm32-telemetry
git submodule update --init --recursive
```

If you already cloned the repository, run the submodule command from its root.

Follow the [firmware guide](firmware/README.md) to build and flash the board.

## Project structure

| Folder | Contents |
| --- | --- |
| [firmware/](firmware/README.md) | Main telemetry firmware |
| [lib/](lib/README.md) | Shared drivers and dependencies |
| [tests/](tests/README.md) | Unit and integration tests that run on a PC |
| [docs/](docs/architecture.md) | Firmware architecture and other technical documentation |
| [SubProjects/](SubProjects/README.md) | Separate projects for prototyping and testing |

## Releases

See [how to make a release](release.md).

## Next steps

- [ ] Read the onboard IMU.
- [ ] Verify SD logging on the board and in the car.
- [ ] Complete the telemetry MVP.
