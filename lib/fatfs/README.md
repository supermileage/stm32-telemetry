# FatFs integration

FatFs R0.12c comes from the pinned `upstream` submodule. The files in this
folder wrap those sources for CubeIDE; CMake builds the upstream sources
directly. Keep `ffconf.h` and the matching FatFs version together.

## Configuration

Read/write FAT12/16/32 is enabled for one volume with 512-byte sectors and
short filenames. exFAT, long filenames, formatting, fast seeking, and RTC
timestamps are disabled. The original licenses are kept alongside the code.

## SD connection

ST's `diskio.c` dispatches requests to the callbacks registered by
`App/storage/sd_diskio.c`. Register before mounting and allow four bytes for
the drive path, including its terminator. Use the supplied dispatcher as the
single implementation of `disk_*`.

The storage task owns all FatFs calls because `_FS_REENTRANT = 0`. Sensor
tasks submit records through the [logger](../../firmware/App/logging/README.md).
For wiring and build setup, see [storage](../../firmware/App/storage/README.md).

CubeIDE links the root `lib` folder as `Lib` and uses `../../lib/fatfs` from
the build directories. Refresh the project after changing `.project` or
`.cproject` outside the IDE.
