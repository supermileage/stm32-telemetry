# Sensor records

Sensor tasks collect samples, encode their payloads, and send records to the
logger. The logger handles the queue and file writes without interpreting the
sensor data.

Source IDs are defined in `sources.h`, with `LOG_SOURCE_IMU` (1) reserved for IMU
data. Keep IDs stable for decoders; an incompatible payload change needs a new ID.

To log a sample, zero-initialize a `log_record_t` and fill in its timestamp in
milliseconds since boot, source ID, and encoded payload. Set `length` to the
number of payload bytes, up to `LOG_PAYLOAD_MAX`, then call `logger_submit()`
from the acquisition task.

The queue copies the record, so the task can reuse it immediately. A false
return means the logger is unavailable, the queue is full, or the record is
invalid.

Keep sample types and encoders in the sensor module. Document each payload's
units, byte order, and field sizes before using it in logs. Do not write native
C structs directly: padding and enum sizes are not a stable file format.

See [storage and logging](../storage/storage_logging.md) for startup, ISR
submission, status, and shutdown, and the [binary format](../logging/logging_format.md)
for framing and CRC details.
