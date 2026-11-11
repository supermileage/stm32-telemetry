# Sensors

Sensor tasks timestamp measurements and send them to the logger queue. GPS is
working; the IMU has a reserved source ID but no task or measurement type yet.

## Submit a measurement

Read the [shared clock](../time/README.md) when a measurement is ready, then set
the source and matching data member:

```cpp
log_record_t record{};
record.timestamp = sample_clock_now();
record.source = LOG_SOURCE_GPS;
record.data.gps = measurement;
bool accepted = logger_submit(&record);
```

The queue copies the record, so you can reuse it immediately. Submission does
not wait for space: check `accepted` and count rejected samples. See
[logging](../logging/README.md) for queue behavior and shutdown.

## GPS reception

The GNSS task reads the SAM-M8Q through USART3 at 9600 baud, with 8 data bits,
no parity, and one stop bit. It must be the port's only reader.

`gnss_task.cpp` passes queued UART bytes to `GnssParser`, which checks checksums
with minmea and decodes RMC sentences. The task uses the first valid GPS calendar
to initialize the RTC, timestamps the decoded measurement, and submits it to the
logger. Failed RTC initialization is retried on a later valid message, even if
logging is unavailable.

The parser has no hardware, RTOS, logger, or clock calls. Measurement types are
in `gnss_record.h`; parser results and `gnss_sample_t` are in `gnss_parser.hpp`.

RTC initialization requires status A and a valid date and time. RMC years 00 to
79 mean 2000 to 2079; 80 to 99 mean 1980 to 1999 and are outside the RTC's range.
Fractional seconds are truncated to milliseconds. Missing fields, impossible
dates, and leap seconds cannot set the clock.

GPS timestamps mark when decoding finishes, including UART buffering and task
delays. See the [clock guide](../time/README.md) for timing limits and the
[CSV format](../logging/README.md#gps-csv-format) for saved columns.

## Reception errors and status

The parser rejects bad checksums, malformed or damaged sentences, and lines too
long for its buffer. A new `$` starts a fresh sentence. UART errors or dropped
bytes cause the task to discard the partial sentence.

`gnss_get_status()` reports received and rejected records, parser errors, dropped
bytes, UART errors, and reception state. Its clock counters are:

- `rtc_initializations`: successful RTC setup, at most once per boot.
- `rtc_init_failures`: failed RTC setup attempts.
- `invalid_utc_sentences`: RMC messages without a usable GPS calendar.

## Add a sensor

1. Define a plain measurement struct. Document units and missing or invalid values.
2. Add a source tag and matching `data` union member in `logging/record.h`.
3. Add a formatter and header in `logging/csv_format.*`. Use
   `csv_format_timestamp()` to start each row and put units in column names.
4. Add a static `CsvFile` and an entry in `logger.cpp`'s `sensor_files` table.
5. Check [FatFs's `_FS_LOCK`](../../config/fatfs/README.md). It allows two open
   files; each CSV needs one slot.

The source must match the filled union member; the logger cannot check this for
you. Records must be trivially copyable, which is checked at compile time. CSV
output does not need packed structs or byte-order conversion.

## Memory and board checks

On ARM, the UART queue uses 2048 bytes plus control data, the parser uses 132
bytes, and the GNSS stack is 2 KiB. Timestamps are 24 bytes and log records are
56 bytes. Recheck sizes when adding fields.

- Check that GPS sets the RTC once, the clock keeps running without GPS, and
  reconnecting GPS does not reset it.
- Measure RTC drift over the expected recording duration.
- Watch dropped bytes, rejected records, RTC failures, and stack headroom with
  `uxTaskGetStackHighWaterMark()`.

Use a logic analyzer for UART timing. Breakpoints can interrupt reception.
