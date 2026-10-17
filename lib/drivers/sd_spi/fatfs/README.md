# STM32Cube FatFs adapter

This adapter connects the [SD SPI driver](../README.md) to ST's
`ff_gen_drv`/`diskio` dispatcher on drive `0:/`. Board setup and application
mount policy stay outside the adapter.

## Build

Compile `sd_diskio.c` alongside the SD core and the configured
[FatFs R0.12c sources](../../../fatfs/README.md). Add the adapter, SD core, and
FatFs header directories to the include path. [sd_diskio.h](sd_diskio.h)
supports C and C++ callers.

Build exactly one copy of ST's `diskio.c` and `ff_gen_drv.c`; do not define
another set of global `disk_*` functions. The `sd_spi` CMake target does not
include this adapter.

The supplied configuration uses one volume, 512-byte sectors, and enabled
writes and ioctls. Formatting and TRIM are disabled.

## Mount and unmount

Keep the `FATFS` object and drive-path buffer alive for the volume's lifetime.
Use one task for adapter, FatFs, and SD driver calls.

1. Configure the board peripherals and bind the SD SPI port.
2. Call `sd_diskio_register(path)` with a writable four-byte buffer.
3. Call `f_mount(&filesystem, path, 1U)` to initialize and mount the card.
4. Read and write files through FatFs.
5. Sync and close all files, then call `f_mount(NULL, path, 0U)`.
6. After a successful unmount, call `sd_diskio_unregister(path)`.

Registration returns `"0:/"`; it does not configure SPI, initialize the card,
or mount the filesystem. It rejects NULL, duplicate registration, another
attached driver, or a dispatcher link failure.

If mounting fails, unmount and unlink before trying a fresh registration.
Check cleanup results. Unregistration clears the path on success and retains
registration state if unlinking fails. It does not close files, unmount, or
clear the SD driver's readiness.

ST's dispatcher caches successful initialization. After an I/O failure, close
files, unmount, then unlink and re-register before remounting so initialization
runs again. The [storage guide](../../../../docs/storage_logging.md) describes
the application's startup and shutdown behavior.

## Disk callbacks

| Callback or command | Behavior |
| --- | --- |
| Initialize | Calls `sd_spi_initialize()`; reports `STA_NOINIT` on failure. |
| Status | Reports cached readiness, without sensing card presence or write protection. |
| Read / write | Forwards sector requests to the SD core. |
| `CTRL_SYNC` | Calls `sd_spi_sync()`; a NULL buffer is allowed. |
| `GET_SECTOR_COUNT` | Reads CSD capacity into a `DWORD`. |
| `GET_SECTOR_SIZE` | Returns 512 through a `WORD`. |
| `GET_BLOCK_SIZE` | Returns one sector through a `DWORD` for alignment, not the measured erase size. |

Core results map to `RES_OK`, `RES_ERROR`, `RES_NOTRDY`, or `RES_PARERR`.
Nonzero drive numbers, unknown ioctls, and NULL output buffers return
`RES_PARERR`. Supported ioctls return `RES_NOTRDY` when the core is not ready.
