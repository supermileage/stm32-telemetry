# Telemetry logging

Sensor tasks copy records into a queue. One storage task writes them to the
SD card through FatFs. The C implementation is covered by host tests, but
`app_init()` does not start it and the CMake firmware target does not include
its sources yet. The usage below describes that implementation once connected.

The logger has a static queue of 64 commands, a 1024-entry task stack
(4 KiB on this target), and a 4096-byte write buffer. It creates the first
unused `LOGxxxxx.BIN` and rotates before exceeding 64 MiB. Existing files
are kept. Normal startup also runs the [storage self-test](../storage/README.md).

## Submit records

Start storage before producers. Include `logging/logger.h`, choose a source
ID for each payload format, and encode fields explicitly. For example, in C:

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

Records are copied, so the caller can reuse the input immediately. Payloads
can hold 0 to 64 bytes. Timestamps are milliseconds since boot and wrap after
about 49.7 days. Convert RTOS ticks if the tick rate changes from 1000 Hz.

`logger_submit()` never waits for queue space. It returns false for invalid
input, a full queue, or an unavailable logger. A full queue rejects the newest
submission and keeps earlier commands. Records can be queued during startup,
but a later mount failure can still discard them. A true return means queued;
`synced_records` tracks successful sync progress.

For interrupts at a FreeRTOS syscall-safe priority (currently numerical
priority 5 or greater):

```c
BaseType_t woken = pdFALSE;
bool accepted = logger_submit_from_isr(&record, &woken);
portYIELD_FROM_ISR(woken);
```

Keep all FatFs and SD calls in the storage task; SPI1 is dedicated to the card.

## Flush, stop, and status

The task syncs about once per second, including when the queue stays busy.
Card latency and higher-priority tasks can delay it. `logger_request_flush()`
queues a sync after earlier records. Capture `accepted_records` first, then
wait for `synced_records` to reach that count. Retry later if the queue is full.

To shut down, stop producers and call `logger_request_stop()`, retrying if
full. Once accepted, the logger rejects new submissions, drains earlier
commands, closes the file, and unmounts the card. Wait for `LOGGER_STOPPED`
before removing the card or cutting power. `LOGGER_FAILED` reports an I/O or
shutdown error. Restarting after either state requires a reboot.

Call `logger_get_status()` from task context:

| Field | Meaning |
| --- | --- |
| `state` | Not started, starting, ready, stopping, stopped, or failed |
| `accepted_records` | Queued records |
| `processed_records` | Records passed to the writer; some may still be buffered |
| `synced_records` | Records covered by the last successful sync or close |
| `dropped_records` | Rejected submissions, excluding accepted records lost on failure |
| `invalid_records` | Invalid pointer or length submissions |
| `queued_commands` | Pending records and flush/stop requests |
| `filesystem_error` | FatFs result from shutdown, or zero |
| `path` | Current log filename |

Disk-full and write errors stop logging without retrying partial writes.
Periodic sync limits buffered loss, but sudden power loss can still damage
the filesystem. Frame CRCs detect corruption; they do not repair it.

The [binary format](../../../docs/logging_format.md) defines the header,
record layout, and CRC. `tools/log_decoder.py` is absent from this checkout,
so there is no decoder command to run yet. Its existing Python tests are
included by CMake only when that script is available. Run the
[host tests](../../../tests/README.md) to check the writer and queue behavior.
