#!/usr/bin/env python3
"""Prevent an updater package from selecting a DSP .bin as firmware."""
import hashlib
import struct
import sys
import tarfile
from pathlib import Path

archive = Path(sys.argv[1])
version = sys.argv[2] if len(sys.argv) > 2 else 'n_261109_ar04'
compat = int(hashlib.md5((version + '\n').encode()).hexdigest()[:8], 16)
with tarfile.open(archive) as tar:
    members = tar.getmembers()
    bins = [m for m in members if m.isfile() and m.name.endswith('.bin')]
    firmware = bins[-1]
    assert firmware.name == f'FIRMWARE/portapack-mayhem_{version}.bin'
    assert firmware.size == 1048576
    image = tar.extractfile(firmware).read()
    assert image[0x410:0x430].split(b'\0', 1)[0].decode() == version
    assert sum(struct.unpack('<262144I', image)) & 0xffffffff == 0
    app_count = 0
    for member in members:
        assert not member.name.startswith('/') and '..' not in Path(member.name).parts
        if member.name.endswith('.ppma'):
            data = tar.extractfile(member).read()
            assert struct.unpack_from('<I', data, 12)[0] == compat, member.name
            assert sum(struct.unpack('<' + 'I' * (len(data) // 4), data)) & 0xffffffff == 0
            app_count += 1
    assert app_count == 104
print(f'PASS update archive: {app_count} matching apps; last .bin is the checksummed {version} firmware')
