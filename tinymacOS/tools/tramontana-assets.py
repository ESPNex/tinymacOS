#!/usr/bin/env python3
"""Pack user-supplied PNG artwork into a multi-resolution runtime atlas outside
of the source workspace. Original PNGs and generated size-specific PNGs are also
installed as ordinary filesystem resources. Application artwork is imported; system symbols remain procedural.
Only unchanged UI controls/cursor/branding use the legacy control sprite sheet.
"""
from pathlib import Path
from PIL import Image
import hashlib,json,struct,sys
D=Path(sys.argv[1]);a=Path('assets');source=a/'icons';m=json.loads((source/'manifest.json').read_text());APP_SLOTS={1,2,3,4,5,13,16,17,18,19,39,40,41,42}
mapping={x['slot']:x['file'] for x in m['icons'] if x['slot'] in APP_SLOTS}
# All folder/file/drive/library/settings/control symbols use the original sheet.
# Installer uses original generated raster artwork, not procedural vector art.
installer_meta=json.loads((source/'installer-generated.json').read_text())
installer=source/installer_meta['file']
assert hashlib.sha256(installer.read_bytes()).hexdigest()==installer_meta['sha256']
assert Image.open(installer).mode=='RGBA'
mapping[38]=installer.name
SIZES=(16,24,32,48,64,128);header=bytearray(43*len(SIZES)*4);body=bytearray();seen={};legacy=(a/'symbols.rgba').read_bytes();base64=[]
for slot in range(43):
    name=mapping.get(slot)
    if name:
        path=source/name
        raw=path.read_bytes();image=Image.open(path).convert('RGBA')
        if slot!=38:
            expected=next(x['sha256'] for x in m['icons'] if x['file']==name);assert hashlib.sha256(raw).hexdigest()==expected
    else:image=Image.frombytes('RGBA',(64,64),legacy[slot*16384:(slot+1)*16384])
    base64.append(image.resize((64,64),Image.Resampling.LANCZOS).tobytes())
    for tier,size in enumerate(SIZES):
        key=(name,size) if name else ('control',slot)
        actual=size if name else 64
        if key not in seen:
            pixels=image.resize((actual,actual),Image.Resampling.LANCZOS)
            seen[key]=len(header)+len(body);body+=pixels.tobytes()
            if name:
                dst=D/'icon-png'/name.removesuffix('.png');dst.mkdir(parents=True,exist_ok=True);pixels.save(dst/f'Icon{size}.png',optimize=True)
        offset=seen[key]
        # High bit marks legacy 64px control artwork, not a variable tier.
        struct.pack_into('<I',header,(slot*len(SIZES)+tier)*4,offset|(0x80000000 if not name else 0))
(D/'icons.atlas').write_bytes(header+body);(D/'icons64.rgba').write_bytes(b''.join(base64))
(D/'icon-atlas.json').write_text(json.dumps({'sizes':SIZES,'slots':mapping,'bytes':len(header)+len(body),'sha256':hashlib.sha256(header+body).hexdigest()},indent=2)+'\n')
print('Imported icon atlas:',len(set(mapping.values())),'artworks,',len(mapping),'slots,',len(SIZES),'sizes')
