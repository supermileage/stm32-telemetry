# Binary logging format

Version 1 logs are written by `firmware/App/logging/log_file.c` and decoded
by `tools/log_decoder.py`. All integer fields are unsigned and little endian.
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

The header is 16 bytes and has no checksum. The decoder requires all values
above to match before reading records.

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
[logging API](../firmware/App/logging/README.md).

From the repository root, replace the example path with your log file:

```bash
python tools/log_decoder.py path/to/LOG00000.BIN -o telemetry.csv
```

CSV columns are `timestamp_ms`, `source`, and `payload_hex`. A bad header,
invalid length, CRC mismatch, or truncated frame stops decoding with exit
code 1. Previously exported valid records remain in the CSV. An empty stream
after a valid file header is a valid file with zero records. The decoder
does not skip damaged frames or repair the filesystem.
