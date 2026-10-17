# SD storage and logging

The storage task handles all FatFs and SD driver calls on SPI1. Sensor tasks
send it records through the logger queue.

## Startup modes

Choose one start function before the scheduler starts:

| Function | Behavior |
| --- | --- |
| `storage_start_task()` | Mount, write and read back a text file, then open the binary log |
| `storage_start_hello_test()` | Run the text-file check and unmount, without starting the logger |

`app_init()` calls `storage_start_task()`. To run only the card check, replace it
with `storage_start_hello_test()`. The first successful task creation sets the
mode for that boot; reboot to restart after shutdown or failure.

Static allocation provides a 64-command queue, 1024-entry stack (4 KiB), and
4096-byte write buffer. Records, flushes, and stops share the queue. Logs use
the first unused `LOGxxxxx.BIN` name and rotate before exceeding 64 MiB.
Existing files are kept.

## Check the card

Use a FAT32-formatted card and the [SD wiring](hardware.md#sd-card). After
startup, inspect these variables in the debugger:

| Variable | Expected result |
| --- | --- |
| `storage_bringup_state` | `STORAGE_READY` in normal mode; `STORAGE_TEST_PASSED` after the one-shot test |
| `storage_mount_result` | `FR_OK` (0) |
| `storage_verify_result` | `FR_OK` (0) |
| `storage_unmount_result` | `FR_OK` after cleanup; initially `FR_NOT_READY` |
| `storage_test_path` | `0:/HELLO000.TXT`, or the next unused number |

## Submit records

Include `logging/logger.h`, choose a source ID for each payload format, and
encode its fields explicitly. This C example logs a 16-bit sample:

```c
bool log_sample(uint16_t sample, uint32_t timestamp_ms)
{
    log_record_t record = {
        .timestamp_ms = timestamp_ms,
        .source = 1U,
        .length = 2U,
        .payload = { (uint8_t)sample, (uint8_t)(sample >> 8) }
    };
    return logger_submit(&record);
}
```

The queue copies the record, so the caller can reuse it immediately. Payloads
hold 0 to 64 bytes. Use milliseconds since boot for timestamps, converting RTOS
ticks if the tick rate changes from 1000 Hz. See the [binary format](logging_format.md).

`logger_submit()` returns true if queued, or false for invalid input, a full
queue, or an unavailable logger. It never waits for space; a full queue rejects
the newest record. Wait for `LOGGER_READY` when possible: submissions accepted
during startup are lost if mounting fails.

From an interrupt, use the ISR API at a FreeRTOS syscall-safe priority
(currently numerical priority 5 or greater):

```c
BaseType_t woken = pdFALSE;
bool accepted = logger_submit_from_isr(&record, &woken);
portYIELD_FROM_ISR(woken);
```

In C++, wrap storage and logger includes in `extern "C"`.

## Flush and stop

The task syncs about once per second, including under load. Card latency and
scheduling can delay it. To flush explicitly, capture `accepted_records`, call
`logger_request_flush()`, and wait for `synced_records` to reach that count,
allowing for counter wrap. Retry later if the queue is full.

To shut down:

1. Stop producers.
2. Call `logger_request_stop()`; retry later if the queue is full.
3. Wait for `LOGGER_STOPPED` before removing the card or cutting power.

An accepted stop rejects new records, drains the queue, closes the file, and
unmounts. `LOGGER_FAILED` reports a startup, I/O, or cleanup error. The task idles
after shutdown because `heap_1` cannot free task memory.

Call `logger_get_status()` from task context to check progress:

| Field | Meaning |
| --- | --- |
| `accepted_records` | Queued, including records already processed |
| `processed_records` | Passed to the writer; may still be buffered |
| `synced_records` | Covered by a successful sync or close |
| `dropped_records` | Valid submissions rejected; excludes accepted records lost on failure |

The snapshot also includes state, invalid submissions, pending commands, the
FatFs error, and the log path. See [logger.h](../firmware/app/logging/logger.h)
for all fields. Counters are 32-bit and wrap. A successful sync cannot guarantee
survival of sudden power loss or FAT damage.

## Troubleshooting

| Result | Check |
| --- | --- |
| `FR_NOT_READY` | Card presence, SPI setup, wiring, and power |
| `FR_NO_FILESYSTEM` | FAT12/16/32 formatting; exFAT and firmware formatting are disabled |
| `FR_DISK_ERR` | Transfers and read-back; inspect the driver's [error counters](../lib/drivers/sd_spi/README.md) |

Disk-full and write errors stop logging; partial writes are not retried.
Fix the cause and reboot. A known issue in
[`log_file_close()`](../firmware/app/logging/log_file.c) clears the open flag
even when `f_close()` fails and FatFs retains the file object. Reboot after a
failed close.

On the board, test missing and full cards, unique filenames after reboot,
orderly shutdown, and sustained producer load. Measure dropped records, queue
use, write/sync latency, and stack headroom before choosing sampling rates.
