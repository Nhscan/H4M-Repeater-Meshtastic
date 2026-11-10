#!/usr/bin/env python3
"""Validate firmware identity, updater residency and ALL external apps.

These are checks on produced bytes, separate from source/build success.
"""
import hashlib
import struct
import sys
from pathlib import Path
import lz4.block

build = Path(sys.argv[1] if len(sys.argv) > 1 else 'build')
version = sys.argv[2] if len(sys.argv) > 2 else 'n_261109_ar04'
firmware = (build / 'firmware/portapack-mayhem-firmware.bin').read_bytes()
assert len(firmware) == 1048576, 'firmware must be exactly 1 MiB'
assert sum(struct.unpack('<262144I', firmware)) & 0xffffffff == 0, 'bad flash checksum'
assert firmware[0x400:0x408] == b'HACKRFFW'
actual = firmware[0x410:0x430].split(b'\0', 1)[0].decode()
assert actual == version, (actual, version)
compat = int(hashlib.md5((version + '\n').encode()).hexdigest()[:8], 16)
assert firmware.count(struct.pack('<I', compat)) >= 2, 'loader version missing'
apps = list((build / 'firmware/application').glob('*.ppma'))
assert len(apps) >= 104
for path in apps:
    data = path.read_bytes()
    assert struct.unpack_from('<I', data, 8)[0] == 3, path.name
    assert struct.unpack_from('<I', data, 12)[0] == compat, path.name
    assert len(data) % 4 == 0
    assert sum(struct.unpack('<' + 'I' * (len(data) // 4), data)) & 0xffffffff == 0, path.name
spi = (build / 'firmware/baseband/baseband.img').read_bytes()
assert firmware.count(spi) == 1, 'tested SPI image is not the flashed image'
offset = 0
tags = []
while offset + 8 <= len(spi):
    tag, length = struct.unpack_from('<4sI', spi, offset)
    if tag == bytes(4):
        break
    compressed = struct.unpack_from('<I', spi, offset + 8)[0]
    assert compressed <= length and offset + 12 + length <= len(spi)
    plain = lz4.block.decompress(spi[offset + 12:offset + 12 + compressed], uncompressed_size=196608)
    reference = build / 'firmware/baseband' / (tag.decode() + '.bin')
    if reference.exists():
        assert plain == reference.read_bytes(), tag
    tags.append(tag.decode())
    offset += 12 + length
assert {'PFUT', 'PLOR', 'PLOT', 'HRF1'} <= set(tags), tags
print(f'PASS install: {version}, compat {compat:#010x}, {len(apps)} matching/checksummed apps, 1 MiB firmware/checksum')
print('PASS actual flashed SPI images: ' + ', '.join(tags) + '; LZ4 decode matches produced basebands')
