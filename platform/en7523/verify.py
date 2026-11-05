#!/usr/bin/env python3
"""Verify the EN7523 diagnostic image and optionally its stock boot prefix."""
import hashlib
from pathlib import Path
import struct
import sys

image = Path(sys.argv[1]).read_bytes()
# Common across the five distinct reference profiles; only covers 0..0x107.
expected = "1752af1bbce53493c6da3f36203afacb2c50edcaf79cc471cb5a9a9dd106bf98"
assert hashlib.sha256(image[:0x108]).hexdigest() == expected, "boot prefix changed"
assert 0x108 < len(image) < 0xf0000, "invalid image extent"
# The diagnostic body intentionally uses only fixed-width base instructions.
for offset in range(0x108, len(image), 4):
    word, = struct.unpack_from("<I", image, offset)
    assert word & 3 == 3, f"compressed or invalid instruction at {offset:#x}"
if len(sys.argv) > 2 and sys.argv[2]:
    reference = Path(sys.argv[2]).read_bytes()
    assert image[:0x108] == reference[:0x108], "stock prefix mismatch"
print(f"PASS: boot prefix and {len(image)}-byte diagnostic image")
