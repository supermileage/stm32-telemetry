# SD storage

This module mounts and unmounts the SD card using the BSP and FatFs. Only the
logger task calls it. See [logging](../logging/README.md) for recording and shutdown.

Use a FAT32-formatted card; the firmware cannot format it. The card uses SPI1 at
250 kHz with CS on PE12. See [board wiring](../../config/board_wiring.md) for
connections.

## Mount and unmount

Call `storage_mount()` after the scheduler starts. It sets up the card, registers
the disk driver, and mounts FatFs drive `0:`. It returns `FR_OK` on success or
if already mounted; otherwise it returns a setup or FatFs error.

Flush and close all files before calling `storage_unmount()`. It detaches FatFs
and unregisters the driver. It is also safe to call after a failed mount.

If mounting fails, cleanup is attempted and the original error is returned.
If cleanup also fails, retry `storage_unmount()` before mounting again.

## Troubleshooting

| Result | Check |
| --- | --- |
| `FR_NOT_READY` | Card power, wiring, SPI setup, or an unfinished previous cleanup |
| `FR_NOT_ENABLED` | SD disk driver registration |
| `FR_NO_FILESYSTEM` | Card format; exFAT is disabled |
| `FR_DISK_ERR` | SPI transfers, wiring, and the SD driver |
| `FR_INT_ERR` during unmount | Driver unlinking |

In the debugger, `registered` means the disk driver is linked; `mounted` means
FatFs mounted the volume. After fixing a card or wiring problem, reboot to start
another recording.
