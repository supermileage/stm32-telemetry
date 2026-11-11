# Logging

Sensor tasks send measurements to a queue. The logger task saves them to the SD
card, so slow writes do not hold up sensor reception.

| File | Purpose |
| --- | --- |
| `logger.*` | Manage the queue, recording folders, saving, and shutdown |
| `csv_format.*` | Turn measurements into CSV rows |
| `csv_file.*` | Buffer rows and write them with FatFs |
| `record.h` | Define the records copied into the queue |

## Startup and submission

`app_init()` calls `logger_start_task()` before the scheduler starts. The logger
then [mounts the card](../storage/README.md), creates the first unused recording
folder, such as `0:/LOGS/RUN00001`, and opens one CSV per registered sensor.
Existing recordings are kept. GPS is the only active sensor so far.

The logger writes and syncs file headers before reporting `LOGGER_READY`. It
accepts records during `LOGGER_STARTING`, but those records are lost if startup
fails. Reboot to start another recording after a stop or failure.

Call `logger_submit()` from a task. It copies the record and returns immediately.
It returns false for invalid input, a full queue, or an unavailable logger.
Count rejected submissions in the sensor's status. See
[sensors](../sensors/README.md) for an example and steps to add a sensor.

## Sync and stop

Files stay open while recording. Full buffers are written to FatFs, and changed
files are synced about once per second, even during continuous reception.
Scheduling and card delays can make a sync late.

To stop recording:

1. Stop submitting measurements.
2. Call `logger_request_stop()` from a task. Retry if the queue is full.
3. Wait for `logger_get_status()` to report `LOGGER_STOPPED` before removing
   the card or powering off.

Once a stop is accepted, new submissions are rejected. The logger handles all
earlier records, writes the remaining rows, closes the files, and unmounts the
card. `LOGGER_FAILED` means startup, writing, or cleanup failed; reboot to try
again. Sudden power loss can still lose data or damage the filesystem.

No button or control task calls the stop API yet.

## Status

Call `logger_get_status()` from a task to read recording progress. Counters are
32-bit totals since boot and can wrap.

| Field | Meaning |
| --- | --- |
| `state` | Not started, starting, ready, stopping, stopped, or failed |
| `accepted_records` | All records accepted into the queue |
| `processed_records` | Records passed to the writer; some may still be in RAM |
| `synced_records` | Records saved by a successful sync or close |
| `dropped_records` | Valid records rejected because the queue was full or the logger unavailable |
| `invalid_records` | Null records or unsupported sources |
| `queued_commands` | Records and stop requests waiting in the queue |
| `filesystem_error` | First startup, write, or cleanup error; zero until one occurs |
| `path` | Recording folder; empty until created |

Compare `accepted_records` with `synced_records` to see how much has been saved.
`dropped_records` does not count records lost after acceptance. `queued_commands`
does not include the command being processed.

## Buffered file writer

Keep `CsvFile` objects in static storage: each has a 4 KiB buffer, too large for
the logger stack. Only the logger task should use them.

- `open(path, header)` creates a new file, writes the header, and syncs it.
- `append(row)` buffers a row and adds its newline.
- `sync()` writes buffered rows and syncs file data and metadata.
- `close()` writes remaining rows and closes the file, preserving the first error.

Pass headers and rows of 1 to 4095 bytes without line endings. The caller handles
formatting and quoting. A failed `f_open()` leaves no open handle, so another
path can be tried.

A short write returns `FR_DENIED`, which may mean the card is full. After a write
or sync error, further writes are blocked to avoid duplicating partial output.
Close every opened file, even if writing or syncing its header failed. If close
fails, stop using the writer and unmount storage; FatFs may still hold the handle.

The destructor does not close the file. Call `close()` and check its result
before unmounting. The writer uses no heap allocation, exceptions, or RTTI.

## Memory and board checks

On ARM, the logger uses a 4 KiB queue, a 4 KiB stack, and a 4 KiB buffer per
sensor file, plus RTOS and FatFs control data.

- Measure write and sync delays at the expected sensor rates. Watch queue usage,
  rejected records, and stack headroom.
- Check that `synced_records` keeps increasing during continuous reception.
- Test stop requests with a full queue. A rejected stop should leave recording
  active; an accepted stop should save earlier records and unmount the card.
- Test a full card and write, sync, and close failures. Cleanup should attempt
  every open file and keep the first error.
