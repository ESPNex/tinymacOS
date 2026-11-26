#!/usr/bin/env python3
"""Fail the build before packaging an ELF the actual firmware loader rejects.
The native loader deliberately reserves an identity-addressed [16,160)MiB
range. Adding static DMA arrays must not silently create an unbootable ISO.
"""
import struct,sys
from pathlib import Path
for name in sys.argv[1:]:
 p=Path(name);b=p.read_bytes();assert len(b)>=64 and b[:5]==b'\x7fELF\x02',name
 typ,machine=struct.unpack_from('<HH',b,16);entry,off=struct.unpack_from('<QQ',b,24);size,count=struct.unpack_from('<HH',b,54)
 assert typ==2 and machine==62 and size==56 and count and off+size*count<=len(b),name
 executable=False;end=0
 for i in range(count):
  kind,flags,fo,va,pa,fs,ms,align=struct.unpack_from('<IIQQQQQQ',b,off+size*i)
  if kind!=1 or not ms:continue
  assert va==pa and 0x1000000<=pa<=0xa000000 and ms<=0xa000000-pa and fs<=ms and fo+fs<=len(b),f'{name}: load segment {i} falls outside boot/loader.c allocation contract: {pa:#x}+{ms:#x}'
  executable|=bool(flags&1 and pa<=entry<pa+ms);end=max(end,pa+ms)
 assert executable,f'{name}: no executable entry'
 print(f'PASS native loader segment contract: {p.name}, loaded end {end:#x}, {0xa000000-end} bytes below ceiling')
