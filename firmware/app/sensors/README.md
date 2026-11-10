# Sensor records

Sensor tasks read measurements and send them to the logger. The logger copies
records into a queue and writes them to the SD card.

The GNSS task is currently connected to the application. The IMU has a reserved
source ID, but its task has not been added to the main firmware yet.

## Submit a measurement

1. Create a zero-initialized `log_record_t`.
2. Capture its timestamp when the measurement becomes available.
3. Set the source ID from `sources.h`.
4. Encode the measurement into `payload` and set `length`.
5. Call `logger_submit(&record)`.

The payload can hold up to `LOG_PAYLOAD_MAX` bytes, currently 64. A successful
submission copies the whole record, so the sensor can reuse its local record.

`logger_submit()` does not wait for space. It returns `false` if the queue is
full, the logger is unavailable, or the record is invalid. Check this return value
and track rejected samples in the sensor's status.

## Capture the timestamp

Use the [shared clock](../time/README.md) when reading a local sensor:

```c
log_record_t record = {0};
record.timestamp = sample_clock_now();
```

Keep that timestamp with the measurement. Waiting for the logger or SD card must
not change the recorded acquisition time.

For buffered input, use the time saved when the data arrived. The GNSS parser uses
`sample_clock_from_tick()` with the receive tick of the sentence's first `$` byte.
See the [GNSS guide](gnss_logging.md) for details.

## Define the payload

Each source ID identifies a payload format. Document the field order, field sizes,
byte order, and units in the sensor module. Use clear units such as `m/s^2`,
`deg/s`, and `degC`.

Encode fields explicitly. Copying a C struct directly into the payload can include
padding and make files harder to decode. If a format changes incompatibly, give
it a new source ID.

## What reaches the SD card

Queued records contain elapsed time, UTC, and synchronization status. The current
binary writer saves only the lower 32 bits of elapsed time. UTC and its status
will be saved when the CSV logger is connected.

See [storage and logging](../storage/storage_logging.md) for the queue and stop
API, and the [binary format](../logging/logging_format.md) for the file layout.
