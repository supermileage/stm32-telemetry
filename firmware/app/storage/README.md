# SD storage

The C storage task mounts the card, writes and reads back a Hello World file,
and starts the telemetry logger. It is covered by host tests with the real
C++ BSP, but `app_init()` does not start it and CMake's firmware target does
not include the storage/logging sources yet. The checks below apply after
startup is connected.

`storage.c` handles board configuration, mounting, unmounting, and the file
check. `storage_task.c` owns the static task, consumes logger commands, and
handles periodic sync and shutdown. The reusable
[FatFs disk adapter](../../../lib/drivers/sd_spi/fatfs/README.md) handles disk
callbacks; the [BSP](../../BSP/README.md) handles STM32 SPI/GPIO and board wiring.
The adapter's C source and include directory must be added separately from
the protocol-only `sd_spi` CMake target when integrating storage.

## Board check

Use a FAT32-formatted card and a module with 3.3 V compatible signals.
Power the module as its manufacturer specifies and connect a common ground.

| SD signal | STM32 pin |
| --- | --- |
| SCK | PA5 |
| MISO | PB4 |
| MOSI | PG4 |
| CS | PE12 |

SPI1 uses mode 0 and 8-bit transfers. With the current 16 MHz kernel clock,
initialization runs at 250 kHz and normal transfers at 4 MHz. See the
[firmware guide](../../README.md) for build and flash instructions.

After connecting storage startup, inspect these variables in the debugger:

| Variable | Expected in normal logger mode |
| --- | --- |
| `storage_bringup_state` | `STORAGE_READY` |
| `storage_mount_result` | `FR_OK` (0) |
| `storage_verify_result` | `FR_OK` (0) |
| `storage_test_path` | `0:/HELLO000.TXT`, or the next unused number |

The text file contains `Hello World` followed by CRLF. The logger then opens
a new `LOGxxxxx.BIN`, which contains only a header until records arrive.
These startup results describe the file check; later logger failures appear
in `logger_get_status()`.

`storage_start_hello_test()` runs only the text-file check, then unmounts.
Wait for `STORAGE_TEST_PASSED` before removing the card. Choose this function
or `storage_start_task()` once per boot; the first call selects the mode.
The `sd-hello` build preset is not connected to either call yet.

## Errors and shutdown

- `FR_NOT_READY`: check the card, SPI setup, wiring, and power.
- `FR_NO_FILESYSTEM`: use FAT12/16/32. exFAT and formatting are disabled.
- `FR_DISK_ERR`: a transfer or read-back failed. Check the SD driver's
  [error counters](../../../lib/drivers/sd_spi/README.md).

The task tries once per boot. Reset after fixing a problem. Only this task
may access FatFs or the dedicated SPI bus; FatFs is configured without
locking between tasks. Producers must use the logger queue.

Before removing the card in normal mode, stop producers, request a logger
stop, and wait for `LOGGER_STOPPED`. See the
[logging guide](../logging/README.md) for the full sequence and failure status.
