#!/usr/bin/env python3
"""Check the linked, packaged images, not just source constants."""
import struct,sys
from pathlib import Path
build=Path(sys.argv[1] if len(sys.argv)>1 else 'build');base=build/'firmware/baseband';apps=build/'firmware/application'
for app,tag,images in [('auto_repeater',b'ARSV',['PCAP','PREP','PAMA','PNFM','PWFM','PRAT']),('meshtastic',b'MSRV',['PLOR','PLOT'])]:
    data=(apps/(app+'.ppma')).read_bytes();address=struct.unpack_from('<I',data)[0];length=struct.unpack_from('<I',data,80)[0]
    reserve=max((base/(x+'.bin')).stat().st_size for x in images)
    assert data[76:80]==tag
    assert address==0x10080000+reserve,(app,'wrong relocation')
    assert length+reserve<=40960,(app,'code region overflow')
    assert len(data)==length+reserve+4
    assert sum(struct.unpack('<'+'I'*(len(data)//4),data))&0xffffffff==0
    assert data[length:-4]==(base/(tag.decode()+'.bin')).read_bytes()
    print(f'PASS {app}: {length} B app + {reserve} B image = {length+reserve}/40960 B; checksum/relocation valid')
spi=(base/'baseband.img').read_bytes();tags=[];offset=0
while offset+8<=len(spi):
    tag,size=struct.unpack_from('<4sI',spi,offset)
    if tag==b'\0'*4:break
    tags.append(tag);offset+=12+size
assert all(tag in tags for tag in [b'PFUT', b'PLOR', b'PLOT', b'HRF1']), tags
for tag in ['PCAP','PREP','PAMA','PNFM','PWFM','PRAT']:
    image=(base/(tag+'.bin')).read_bytes();assert image and len(image)<=32752
print('PASS SPI updater/LoRa/factory images and SD-loaded radio images')
