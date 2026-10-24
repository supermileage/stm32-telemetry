# FatFs integration

FatFs R0.12c comes from the pinned `upstream` submodule. This folder wraps its
sources for CubeIDE and provides `ffconf.h`. Keep the configuration and FatFs
version together, and compile each source only once.

## Configuration

Read/write FAT12/16/32 is enabled for one volume with 512-byte sectors and
short filenames. exFAT, long filenames, formatting, fast seeking, and RTC
timestamps are disabled. The original licenses are kept alongside the code.

## SD connection

ST's `diskio.c` dispatches requests through the optional
[SD SPI adapter](../drivers/sd_spi/fatfs/README.md) in
`lib/drivers/sd_spi/fatfs/sd_diskio.c`. Register before mounting and allow four
bytes for the drive path, including its terminator. Use the supplied dispatcher
as the single implementation of `disk_*`. Board configuration and application
mount policy are outside this adapter.

The storage task owns all FatFs calls because `_FS_REENTRANT = 0`. Sensor
tasks submit records through the [logger](../../firmware/app/storage/storage_logging.md#logging-sensor-data).
See [hardware](../../docs/hardware.md) for wiring and the
[firmware guide](../../firmware/README.md) for build setup.

CubeIDE links the root `lib` folder as `Lib` and uses `../../lib/fatfs` from
the build directories. Refresh the project after changing `.project` or
`.cproject` outside the IDE.
