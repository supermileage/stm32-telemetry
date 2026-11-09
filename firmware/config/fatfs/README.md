# FatFs configuration

FatFs handles reading and writing files on the SD card.

- [`ffconf.h`](ffconf.h) contains the FatFs settings.
- [`lib/fatfs/src`](../../../lib/fatfs/src) contains the FatFs source files.
- [`sd_diskio.cpp`](../../bsp/storage/sd_diskio.cpp) connects FatFs to the SD card driver.

Use `diskio.c` from `lib/fatfs/src`, not a copy from an SD driver or example. See the [library guide](../../../lib/README.md) for the full list of source files.

**Only the logger task should call FatFs.** Since `_FS_REENTRANT` is disabled, FatFs is not safe to call from multiple tasks. Sensor tasks should send records through `logger.h` instead. Keep all new file operations in the logger task.