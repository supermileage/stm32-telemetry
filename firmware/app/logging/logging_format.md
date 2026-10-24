# Binary logging format

[`log_file.c`](log_file.c) writes version 1 logs as
`LOG00000.BIN` through `LOG99999.BIN`, using the first unused name.
Every file starts with a header. All integers are unsigned and little endian.

## File header

| Offset | Size (bytes) | Value |
| --- | --- | --- |
| 0 | 4 | ASCII `STML` |
| 4 | 2 | Format version: 1 |
| 6 | 2 | Header size: 16 |
| 8 | 4 | Timestamp frequency: 1000 Hz |
| 12 | 4 | Reserved: zero |

Validate all header fields before reading records. The header has no checksum.

## Record frame

| Offset | Size (bytes) | Value |
| --- | --- | --- |
| 0 | 4 | ASCII `REC1` |
| 4 | 4 | Milliseconds since boot |
| 8 | 2 | Producer-selected source ID |
| 10 | 2 | Payload length, 0 through 64 |
| 12 | payload length | Payload bytes |
| 12 + payload length | 4 | CRC-32/ISO-HDLC |

A frame takes `16 + payload_length` bytes, with no padding. The CRC covers
the 12-byte frame header and payload. It uses reflected polynomial
`0xEDB88320`, initial value `0xFFFFFFFF`, and final XOR `0xFFFFFFFF`.
Python's `zlib.crc32(frame_header + payload)` gives the same value; store it
little endian.

Each producer defines a source ID and payload format. Encode fields explicitly
rather than writing a C struct directly. Timestamps restart on boot and wrap
after `2^32` milliseconds (about 49.7 days).

Application source IDs are registered in
[`sensors/sources.h`](../sensors/sources.h). Payload types and
encoders belong to their sensor modules, outside the generic logger.
The [IMU payload format](../sensors/README.md#imu-vector-payload)
uses source ID 1. Incompatible payload changes require a new source ID.

## Reading logs

After validating the file header, read frames in order and check the `REC1`
marker, payload length, and CRC. Stop at the first corrupt or incomplete frame
and keep earlier valid records. A valid header with no records is an empty log.

A PC decoder has not been implemented. Its CSV output should use the columns
`timestamp_ms`, `source`, and `payload_hex`.

See [storage and logging](../storage/storage_logging.md) for buffering, rotation, and
shutdown. CRCs detect damaged records but do not protect FAT metadata from
power loss.
