"""Independent CRC and malformed/torn-record checks for the card log decoder."""
import importlib.util
import io
from pathlib import Path
import struct
import unittest
import zlib

spec = importlib.util.spec_from_file_location(
    "decode_log", Path(__file__).resolve().parents[2] / "tools/log_decoder.py")
decoder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(decoder)
HEADER = struct.pack("<4sHHII", b"STML", 1, 16, 1000, 0)


def frame(timestamp, source, payload):
    prefix = struct.pack("<4sIHH", b"REC1", timestamp, source, len(payload)) + payload
    return prefix + struct.pack("<I", zlib.crc32(prefix))


class DecoderTests(unittest.TestCase):
    def test_round_trip_and_empty_file(self):
        self.assertEqual(list(decoder.read_records(io.BytesIO(HEADER))), [])
        data = HEADER + frame(0xFFFFFFFF, 1, b"") + frame(123, 7, bytes(range(64)))
        self.assertEqual(list(decoder.read_records(io.BytesIO(data))),
                         [(0xFFFFFFFF, 1, ""), (123, 7, bytes(range(64)).hex())])

    def test_torn_tail_preserves_preceding_record(self):
        valid = frame(123, 7, b"\x01\x02")
        tail = frame(124, 7, b"\x03\x04")
        for size in range(1, len(tail)):
            records = decoder.read_records(io.BytesIO(HEADER + valid + tail[:size]))
            self.assertEqual(next(records), (123, 7, "0102"))
            with self.assertRaisesRegex(decoder.LogError, "truncated frame"):
                next(records)

    def test_corrupt_payload_and_crc(self):
        valid = frame(123, 7, b"\x01\x02")
        for offset in (12, len(valid) - 1):
            damaged = bytearray(valid)
            damaged[offset] ^= 1
            with self.assertRaisesRegex(decoder.LogError, "CRC mismatch"):
                list(decoder.read_records(io.BytesIO(HEADER + damaged)))

    def test_bad_frame_magic_and_oversized_payload(self):
        for magic, length in [(b"BAD!", 0), (b"REC1", 65)]:
            bad = struct.pack("<4sIHH", magic, 0, 1, length)
            with self.assertRaisesRegex(decoder.LogError, "invalid frame header"):
                list(decoder.read_records(io.BytesIO(HEADER + bad)))

    def test_bad_file_header(self):
        for data in (HEADER[:8], b"BAD!" + HEADER[4:],
                     struct.pack("<4sHHII", b"STML", 2, 16, 1000, 0)):
            with self.assertRaises(decoder.LogError):
                list(decoder.read_records(io.BytesIO(data)))


if __name__ == "__main__":
    unittest.main()
