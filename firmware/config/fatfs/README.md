# FatFs configuration

FatFs provides the SD card filesystem. Edit its settings in [`ffconf.h`](ffconf.h).
The source is in [`lib/fatfs/src`](../../../lib/fatfs/src), and
[`sd_diskio.c`](../../bsp/storage/sd_diskio.c) connects it to the SD driver.

Only the [logger task](../../app/logging/README.md) should call FatFs. Sensor
tasks send records to the logger instead.

| Setting | Value | Meaning |
| --- | --- | --- |
| `_FS_REENTRANT` | 0 | No locking between tasks |
| `_FS_LOCK` | 2 | Track up to two open files or directories |
| `_USE_LFN` | 0 | Names must fit the short 8.3 format |
| `_FS_NORTC` | 1 | Use a fixed date for file metadata |

`GPS.CSV` uses one open-file slot. Increase `_FS_LOCK` if adding sensors will
leave more than two CSVs open. File dates are separate from the measurement
timestamps saved inside each CSV.

Build with the `diskio.c` in `lib/fatfs/src`. Adding another copy from an SD
example causes duplicate definitions. See [libraries](../../../lib/README.md)
for the source list.
