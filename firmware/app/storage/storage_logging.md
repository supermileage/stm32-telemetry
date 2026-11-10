# SD storage and logging

The logger task writes sensor records to the SD card through FatFs and SPI1.
Sensor tasks send records through a queue. **Only the logger task accesses the
SD card**, including mounting, syncing, closing, and unmounting.

The current logger writes binary files. The `CsvFile` class is ready, but session
folders and per-sensor CSV files have not been connected yet.

## Start and check the logger

`app_init()` calls `logger_start_task()` before the scheduler starts. When the
logger task runs, it mounts the card and opens the first unused `LOGxxxxx.BIN`.
It opens a new file before reaching 64 MiB and never overwrites an existing file.
Use a FAT32-formatted card; the firmware does not format it.

Read `logger_get_status()` from a task to check startup:

| Field | What to look for |
| --- | --- |
| `state` | `LOGGER_READY` when the file is open |
| `filesystem_error` | `FR_OK`, or 0, when no error has been reported |
| `path` | The latest published log filename |

Records can be accepted during `LOGGER_STARTING`, but they may be lost if mounting
or opening the file fails. Wait for `LOGGER_READY` when practical.

The main logger allocations are a 64-command queue using 6,656 bytes, a 4 KiB task
stack, and a 4 KiB write buffer. RTOS and FatFs control objects use additional
memory. No separate storage task is used.

## Send sensor data

Create a zero-initialized `log_record_t`, capture its acquisition timestamp, then
fill in the source ID, payload, and length. Payloads can hold up to 64 bytes.
See the [sensor guide](../sensors/README.md) for the steps.

`logger_submit()` returns `true` when the record is copied into the queue. It
returns `false` for invalid records, a full queue, or an unavailable logger.
It never waits for queue space, so storage delays do not block sensor tasks.

The queue keeps elapsed time, UTC, and time status with each sample. The current
[binary format](../logging/logging_format.md) saves only the lower 32 bits of
elapsed time. UTC will reach the card when CSV recording is connected.

### From an interrupt

Use the ISR version of the API:

```c
BaseType_t woken = pdFALSE;
bool accepted = logger_submit_from_isr(&record, &woken);
portYIELD_FROM_ISR(woken);
```

Check `accepted` to track a rejected sample. The interrupt priority must satisfy
`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` in `FreeRTOSConfig.h`. This call
only queues the record; it does not write to the SD card.

## Sync and stop

The logger writes buffered data and syncs about once per second. Card latency
and scheduling can delay a sync. Use `logger_request_flush()` to request one
after earlier queued records. A `true` return means the request was queued,
not that the sync has finished. Retry if the queue is full.

Before removing the card or powering off:

1. Stop submitting sensor records.
2. Call `logger_request_stop()`. If the queue is full, retry while the logger is
   still running.
3. After the request is accepted, wait for `LOGGER_STOPPED`.

An accepted stop rejects further records, finishes earlier queued records, closes
the file, and unmounts the card. A failure during startup, writing, or cleanup
leaves the logger in `LOGGER_FAILED` instead.

Reboot to restart logging after a stop or failure. Calling `logger_start_task()`
again does not restart the task. A successful sync reduces data loss, but sudden
power loss can still damage files or the filesystem.

## Understand the counters

| Field | Meaning |
| --- | --- |
| `accepted_records` | Records copied into the queue |
| `processed_records` | Records passed to the writer; some may still be buffered |
| `synced_records` | Records covered by a successful sync or close |
| `dropped_records` | Valid records rejected during submission |
| `invalid_records` | Invalid records or ISR wake pointers |
| `queued_commands` | Waiting records, flush requests, and stop requests |

Compare accepted and synced counts to see how far saving has progressed. Records
accepted before an I/O failure are not added to `dropped_records`. A queued record
is not a guarantee that it reached the card. Counters are 32-bit and can wrap.

`filesystem_error` reports the first startup, I/O, or cleanup error.
`queued_commands` excludes the command currently being processed.

## Mounting and cleanup

`storage_mount()` checks the SPI setup, links the SD driver, then mounts drive
`0:/` with FatFs. A repeated call while mounted returns `FR_OK`. If setup fails,
storage attempts to detach FatFs and unlink the driver. Unfinished cleanup stays
tracked so the logger can attempt it again while preserving the original error.

The logger closes its file before calling `storage_unmount()`. Keep these calls
in the logger task so shutdown follows the same order as normal file access.

## Troubleshooting

| Error | Check |
| --- | --- |
| `FR_NOT_READY` | Card power, wiring, and SPI setup |
| `FR_NOT_ENABLED` | SD driver registration; drive 0 must be available |
| `FR_NO_FILESYSTEM` | Card formatting; exFAT is disabled |
| `FR_DISK_ERR` | SPI transfers, wiring, and the SD driver |
| `FR_INT_ERR` during unmount | FatFs detach or SD driver unlinking |

For a mount failure, step through `storage_mount()` in the debugger. The
`registered` flag means the driver is linked; `mounted` means the mount completed.
A full card or write error stops logging. Fix the problem and reboot.

## Check on the board

- Run at the highest sensor rate and watch queue usage and `dropped_records`.
- Check stack headroom with `uxTaskGetStackHighWaterMark()`.
- Confirm `synced_records` increases while records arrive continuously.
- Fill the queue and request a stop. A rejected request should leave logging
  running; an accepted one should finish earlier records and reject new ones.
