# STM32Cube FatFs adapter

Optional adapter between the [SD SPI block driver](../README.md) and ST's
`ff_gen_drv`/`diskio` dispatcher. It contains no board, HAL, RTOS, mount-policy,
or telemetry dependencies. It registers one card on logical drive `0:/`.

## Dependencies and build

Compile `sd_diskio.c` as C alongside the SD SPI core and the configured
[FatFs R0.12c integration](../../../fatfs/README.md). Add this directory, the SD
SPI directory, and the FatFs public headers to the include path. Both C and C++
callers can include [sd_diskio.h](sd_diskio.h).

The adapter requires ST's `Diskio_drvTypeDef`, `FATFS_LinkDriver()`,
`FATFS_UnLinkDriver()`, and `FATFS_GetAttachedDriversNbr()`. It is not a generic
replacement for every FatFs release's `diskio` layer. Build exactly one copy of
ST's `diskio.c` and `ff_gen_drv.c`; do not also define global `disk_*` functions.
The `sd_spi` CMake target builds only the protocol core.

For an application that already provides a configured `fatfs` CMake target:

```cmake
target_sources(application PRIVATE path/to/sd_spi/fatfs/sd_diskio.c)
target_include_directories(application PRIVATE path/to/sd_spi/fatfs)
target_link_libraries(application PRIVATE sd_spi fatfs)
```

The supplied configuration uses one volume, 512-byte sectors, `_USE_WRITE = 1`,
`_USE_IOCTL = 1`, and disabled formatting/TRIM. Changing those options requires
reviewing the adapter callback table and ioctl behavior.

## Registration and mount lifecycle

1. Initialize board peripherals and bind the SD SPI port.
2. Call `sd_diskio_register(path)` with a writable four-byte path buffer.
3. Call `f_mount(&filesystem, path, 1U)` to initialize and mount immediately.
4. Access files through FatFs from the owning task.
5. Sync and close all files, then call `f_mount(NULL, path, 0U)` to unmount.
6. After a successful unmount, call `sd_diskio_unregister(path)`.

Registration returns `"0:/"` and does not configure SPI, initialize the card, or
mount a filesystem. It rejects NULL, duplicate registration, any already-attached
driver, or a failed dispatcher link. A mount failure must be followed by unmount
and unlink cleanup before attempting a fresh registration.

Unregistration clears the path on success. It does not close files, unmount,
power down the card, or clear the core driver's ready state. A failed unlink
retains registration state. Retain the `FATFS` object and path for the volume's
lifetime. Serialize adapter, filesystem, and core driver calls together.

For example, this mount helper leaves unmount/unlink cleanup to its caller even
if `f_mount()` fails:

```c
#include "sd_diskio.h"
#include "ff.h"

static FATFS filesystem;
static char drive_path[4];

FRESULT mount_card(void)
{
    /* Platform SPI port must already be configured and bound. */
    if (!sd_diskio_register(drive_path)) {
        return FR_NOT_ENABLED;
    }
    return f_mount(&filesystem, drive_path, 1U);
}
```

## Disk interface

| Callback or command | Behavior |
| --- | --- |
| Initialize | Calls `sd_spi_initialize()`; returns `STA_NOINIT` unless successful. |
| Status | Reports cached SD readiness; no card-detect or write-protect sensing. |
| Read / write | Forwards sector requests and converts the core result. |
| `CTRL_SYNC` | Calls `sd_spi_sync()`; a NULL buffer is accepted. |
| `GET_SECTOR_COUNT` | Reads CSD capacity into a `DWORD`. |
| `GET_SECTOR_SIZE` | Returns 512 through a `WORD`. |
| `GET_BLOCK_SIZE` | Returns one sector through a `DWORD` as an allocation-alignment fallback, not the card's measured erase size. |

Callbacks reject nonzero drive/LUN values. Unknown ioctls and NULL output buffers
return `RES_PARERR`. Supported ioctls return `RES_NOTRDY` when the core is not
ready. Formatting and TRIM are not implemented by this adapter.

| Core result | FatFs disk result |
| --- | --- |
| `SD_OK` | `RES_OK` |
| `SD_ERROR` | `RES_ERROR` |
| `SD_NOT_READY` | `RES_NOTRDY` |
| `SD_INVALID_ARGUMENT` | `RES_PARERR` |

ST's dispatcher caches successful initialization. After an I/O error, close
files and unmount, then unlink and re-register before remounting so card
initialization runs again. Applications own recovery and must check cleanup
results. The [storage integration](../../../../firmware/app/storage/README.md)
and [SD/FatFs tests](../../../../tests/sd_spi/README.md) show the surrounding
lifecycle.
