# Sensor Records

Sensor tasks collect data, encode it into a payload, and send it to the logger. The logger handles queuing and writing records to the SD card without needing to understand the sensor data.

## Source IDs

Each sensor type has a source ID defined in `sources.h`.

For example, `LOG_SOURCE_IMU` (1) identifies IMU data.

Keep source IDs consistent so logged data can be decoded correctly. If a sensor's payload format changes in a way that breaks compatibility, assign it a new source ID.

## Logging Sensor Data

To log a sensor sample:

1. Create a `log_record_t` and initialize it to zero.
2. Set its timestamp in milliseconds since boot.
3. Set the sensor's source ID.
4. Encode the sensor data into the payload.
5. Set `length` to the number of payload bytes.
6. Call `logger_submit()` to send the record to the logger.

The payload must not exceed `LOG_PAYLOAD_MAX` bytes.

The logger queue copies the record, so the sensor task can safely reuse its record after submission.

`logger_submit()` returns `false` if the logger is unavailable, the queue is full, or the record is invalid.

## Payload Format

Each sensor module is responsible for defining its sample data and encoding it into a payload.

When defining a payload, document:

- **Units:** Such as m/s², °C, or degrees/s.
- **Byte order:** How multi-byte values are stored.
- **Field sizes:** How many bytes each value uses.
- **Field order:** The order values appear in the payload.

Do not write C structs directly into the payload. Struct padding and data type sizes can vary, making the logs harder to decode reliably.

## Related Documentation

- [Storage and Logging](../storage/storage_logging.md) — Logger startup, queue handling, ISR submission, status, and shutdown.
- [Binary Log Format](../logging/logging_format.md) — Record structure, framing, and CRC validation.
