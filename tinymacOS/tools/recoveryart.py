#!/usr/bin/env python3
"""Derive a small static recovery font+icon subset. No desktop graphics linked into Recovery."""
from pathlib import Path
import struct,sys
D=Path(sys.argv[1]);a=Path('assets');raw=(a/'type.tfn').read_bytes();nf,ng=struct.unpack_from('<II',raw,4);end=12+4*ng;bits=end+nf*(4+ng*8);hdr=bytearray(raw[:end]);struct.pack_into('<I',hdr,4,4);px=bytearray()
for i in (0,1,2,3):
 pos=end+i*(4+8*ng);h=struct.unpack_from('<I',raw,pos)[0];hdr+=struct.pack('<I',h)
 for j in range(ng):
  adv,w,o=struct.unpack_from('<HHI',raw,pos+4+8*j);hdr+=struct.pack('<HHI',adv,w,len(px));px+=raw[bits+o:bits+o+w*h]
(D/'rec.font').write_bytes(hdr+px);icons=(D/'icons64.rgba').read_bytes();legacy=(a/'symbols.rgba').read_bytes();(D/'rec.icons').write_bytes(b''.join((icons if i in (2,38) else legacy)[i*16384:(i+1)*16384] for i in (31,33,38,39,2,9,15,17)));(D/'recovery-art.S').write_text('.section .rodata\n.balign 8\n.global recfont,recicons\nrecfont: .incbin "rec.font"\nrecicons: .incbin "rec.icons"\n.section .note.GNU-stack,"",@progbits\n')
