#!/usr/bin/env python3
"""Append the Italian euro glyph to TFN2 without changing any existing glyph.
Idempotent. Normal builds consume the checked-in asset, not host fonts.
"""
from pathlib import Path
import struct
from PIL import Image,ImageDraw,ImageFont
p=Path(__file__).resolve().parents[1]/'assets/type.tfn';raw=p.read_bytes()
nf,ng=struct.unpack_from('<II',raw,4);codes=list(struct.unpack_from(f'<{ng}I',raw,12))
if 0x20ac in codes:raise SystemExit('Euro already present; no changes')
faces=[('DejaVuSans.ttf',12),('DejaVuSans.ttf',14),('DejaVuSans-Bold.ttf',14),('DejaVuSans-Bold.ttf',25),('DejaVuSans.ttf',38),('DejaVuSansMono.ttf',13),('DejaVuSans-Bold.ttf',10),('DejaVuSans.ttf',54),('DejaVuSans.ttf',17),('DejaVuSans-Bold.ttf',86)]
assert nf==len(faces)
start=12+ng*4;bits=start+nf*(4+ng*8);pixels=bytearray(raw[bits:]);tables=[]
for i,(name,size) in enumerate(faces):
 at=start+i*(4+ng*8);height=struct.unpack_from('<I',raw,at)[0];assert height==size+8
 table=bytearray(raw[at:at+4+ng*8]);font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/'+name,size)
 adv=round(font.getlength('€'))+(1 if size==10 else 0);width=max(adv,font.getbbox('€')[2])+1
 image=Image.new('L',(width,height));ImageDraw.Draw(image).text((0,0),'€',font=font,fill=255)
 table.extend(struct.pack('<HHI',adv,width,len(pixels)));pixels.extend(image.tobytes());tables.append(table)
p.write_bytes(b'TFN2'+struct.pack('<II',nf,ng+1)+struct.pack(f'<{ng+1}I',*codes,0x20ac)+b''.join(tables)+pixels)
print('Euro added in',nf,'font faces; all previous glyph descriptors/pixels preserved')
