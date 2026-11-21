#!/usr/bin/env python3
"""Append a genuine weight-600 display face, preserving all old font tables.
The tiny wordmark remains the original DejaVu regular face, unchanged.
"""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import struct,json,hashlib,sys
p=Path('assets/type.tfn');raw=p.read_bytes();nf,ng=struct.unpack_from('<II',raw,4)
if nf==11:
 if '--rebuild' not in sys.argv:raise SystemExit(0)
 end=12+ng*4;step=4+ng*8;new_bits=end+11*step;old_end=0
 for face in range(10):
  at=end+face*step;h=struct.unpack_from('<I',raw,at)[0]
  for j in range(ng):
   adv,w,o=struct.unpack_from('<HHI',raw,at+4+j*8);old_end=max(old_end,o+w*h)
 old=bytearray(raw[:end+10*step]+raw[new_bits:new_bits+old_end]);struct.pack_into('<I',old,4,10);raw=bytes(old);nf=10
assert nf==10
codes=struct.unpack_from(f'<{ng}I',raw,12);end=12+ng*4;bits=end+nf*(4+ng*8)
head=bytearray(raw[:bits]);struct.pack_into('<I',head,4,11);pixels=bytearray(raw[bits:]);font=ImageFont.truetype('assets/font-source/Display-Semibold.ttf',38)
# Noto's em metrics differ: anchor to the same display top as the regular face.
h=48;table=bytearray(struct.pack('<I',h))
for c in codes:
 ch=chr(c);adv=round(font.getlength(ch));w=max(adv,font.getbbox(ch)[2])+1;im=Image.new('L',(w,h));ImageDraw.Draw(im).text((0,-5),ch,font=font,fill=255)
 table.extend(struct.pack('<HHI',adv,w,len(pixels)));pixels.extend(im.tobytes())
p.write_bytes(head+table+pixels)
# Derive the exact unchanged prefix width from the original face's advances.
at=end+4*(4+ng*8);width=sum(struct.unpack_from('<H',raw,at+4+codes.index(ord(c))*8)[0] for c in 'tiny')
Path('assets/font-source/wordmark.json').write_text(json.dumps({'tiny_face':4,'tiny_advance':width,'macOS_face':10,'macOS_weight':600,'old_font_sha256':hashlib.sha256(raw).hexdigest()},indent=2)+'\n');print('Semibold display added; unchanged tiny width:',width)
