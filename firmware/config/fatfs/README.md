# FatFs configuration

FatFs lets the firmware read and write files on the SD card.

[`ffconf.h`](ffconf.h) contains the project settings. The FatFs source is in
[`lib/fatfs/src`](../../../lib/fatfs/src), and
[`bsp/storage/sd_diskio.cpp`](../../bsp/storage/sd_diskio.cpp) connects it to
the SD card driver.

Compile only the `diskio.c` file in [`lib/fatfs/src`](../../../lib/fatfs/src).

Only the storage task should use FatFs. If another task needs to read or write
a file, it should send a request to the storage task. `_FS_REENTRANT` is
disabled, so FatFs cannot safely handle simultaneous access.
