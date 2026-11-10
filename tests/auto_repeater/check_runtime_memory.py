#!/usr/bin/env python3
"""Check runtime allocations that a successful firmware link does not cover.

Requires pyelftools and the ELF with debug information.
"""
import sys
from pathlib import Path
from elftools.elf.elffile import ELFFile

path = Path(sys.argv[1] if len(sys.argv) > 1 else 'build') / 'firmware/baseband/baseband_lora_rx.elf'
with path.open('rb') as stream:
    elf = ELFFile(stream)
    symbols = {s.name: s.entry.st_value for s in elf.get_section_by_name('.symtab').iter_symbols()}
    heap = symbols['__heap_end__'] - symbols['__heap_base__']
    size = None
    for cu in elf.get_dwarf_info().iter_CUs():
        for die in cu.iter_DIEs():
            name = die.attributes.get('DW_AT_name')
            length = die.attributes.get('DW_AT_byte_size')
            if name and name.value == b'LoRaProcessor' and length:
                size = length.value
                break
        if size is not None:
            break
    assert size, 'LoRaProcessor type size missing'
    # Two heap allocations (object + DMA) require allocator headers. Leave
    # room for kernel allocation/alignment and future small runtime objects.
    remaining = heap - size - 16384 - 32
    assert remaining >= 4096, f'M4 runtime heap exhausted: {remaining} bytes remain'
    print(f'PASS LoRa runtime: heap {heap}, processor {size}, DMA 16384, headroom {remaining} bytes')
