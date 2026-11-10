# Binary log format

The current writer creates `LOG00000.BIN` through `LOG99999.BIN`, choosing the
first unused name. Each file has a header followed by record frames. Header and
frame integers are unsigned and little endian. Each sensor defines its payload
separately.

Queued records hold elapsed time, UTC, and time status. This version 1 file format
stores only the lower 32 bits of acquisition elapsed time. The planned sensor CSV
files will also save UTC and its status.

## File header

| Offset | Bytes | Value |
| --- | --- | --- |
| 0 | 4 | ASCII `STML` |
| 4 | 2 | Format version: 1 |
| 6 | 2 | Header size: 16 |
| 8 | 4 | Timestamp frequency: 1000 Hz |
| 12 | 4 | Reserved: zero |

Check all header fields before reading records. The header has no checksum.

## Record frame

| Offset | Bytes | Value |
| --- | --- | --- |
| 0 | 4 | ASCII `REC1` |
| 4 | 4 | Acquisition milliseconds since boot, lower 32 bits |
| 8 | 2 | Sensor source ID |
| 10 | 2 | Payload length, from 0 to 64 |
| 12 | Payload length | Encoded sensor data |
| 12 + payload length | 4 | CRC-32/ISO-HDLC |

A frame uses `16 + payload_length` bytes with no padding. The CRC covers the
12-byte frame header and payload. Its settings are:

- Reflected polynomial: `0xEDB88320`
- Initial value: `0xFFFFFFFF`
- Final XOR: `0xFFFFFFFF`

Python's `zlib.crc32(frame_header + payload)` gives the same value. Store that
result little endian.

## Source IDs and timestamps

Source IDs are listed in [`sources.h`](../sensors/sources.h). Source 1 is reserved
for the IMU; source 2 is GPS. The sensor module defines each payload's field order,
sizes, byte order, and units. See the [sensor guide](../sensors/README.md).

The file's timestamp wraps every `2^32` milliseconds, about 49.7 days. The shared
clock and queued records keep 64-bit elapsed time, but the binary format does not.

## Read a file

After checking the header, read frames in order. Check the `REC1` marker, payload
length, and CRC for each frame. Stop at the first damaged or incomplete frame and
keep the earlier valid records. A valid header with no frames is an empty log.

CRCs detect damaged records; they do not protect filesystem metadata from power
loss. See the [logging guide](../storage/storage_logging.md) for sync and shutdown.
