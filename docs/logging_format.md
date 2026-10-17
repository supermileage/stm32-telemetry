# Binary logging format

Version 1 logs are written by `firmware/app/logging/log_file.c`.
All integer fields are unsigned and little endian.
Files are named `LOG00000.BIN` through `LOG99999.BIN`; existing files are
not overwritten. Each new file starts with its own header.

## File header

| Offset | Size (bytes) | Value |
| --- | --- | --- |
| 0 | 4 | ASCII `STML` |
| 4 | 2 | Format version: 1 |
| 6 | 2 | Header size: 16 |
| 8 | 4 | Timestamp frequency: 1000 Hz |
| 12 | 4 | Reserved: zero |

The header is 16 bytes and has no checksum. A reader should validate these
values before reading records.

## Record frame

| Offset | Size (bytes) | Value |
| --- | --- | --- |
| 0 | 4 | ASCII `REC1` |
| 4 | 4 | Milliseconds since boot |
| 8 | 2 | Producer-selected source ID |
| 10 | 2 | Payload length, 0 through 64 |
| 12 | payload length | Payload bytes |
| 12 + payload length | 4 | CRC-32/ISO-HDLC |

A frame occupies `16 + payload_length` bytes without padding. CRC covers
the frame header and payload, excluding the checksum itself. It uses the
reflected polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, and final XOR
`0xFFFFFFFF`; Python's `zlib.crc32(frame_header + payload)` produces the same
32-bit value. Store the checksum little endian.

Source IDs and payload schemas are defined by producers. Serialize fields
explicitly; C struct layout is not a disk format. Timestamps wrap after
`2^32` milliseconds (about 49.7 days) and restart on each boot.

## Buffering and decoding

The writer buffers 4096 bytes, rotates before exceeding 64 MiB, and syncs
periodically. A successful queue submission is not a persistence guarantee;
use logger status and the stop sequence in the
[logging API](../firmware/app/logging/README.md).

`tools/log_decoder.py` is missing from this checkout. The existing
`tests/logging/test_decode_log.py` defines the expected reader behavior:
validate the header, reject invalid lengths and CRCs, and stop at a corrupt
or truncated frame while retaining earlier valid records. A valid header
with no following records represents an empty log.

The planned CSV fields are `timestamp_ms`, `source`, and `payload_hex`.
There is no decoder command to run until the script is restored or implemented.
