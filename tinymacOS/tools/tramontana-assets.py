#!/usr/bin/env python3
"""Pack user-supplied PNG artwork into a multi-resolution runtime atlas outside
of the source workspace. Original PNGs and generated size-specific PNGs are also
installed as ordinary filesystem resources. Application artwork is imported; system symbols remain procedural.
Only unchanged UI controls/cursor/branding use the legacy control sprite sheet.
"""
from pathlib import Path
from PIL import Image,ImageDraw
import hashlib,json,struct,sys
D=Path(sys.argv[1]);a=Path('assets');source=a/'icons';m=json.loads((source/'manifest.json').read_text());APP_SLOTS={1,2,3,4,5,16,17,18,19,39,40,41,42}
mapping={x['slot']:x['file'] for x in m['icons'] if x['slot'] in APP_SLOTS}
# All folder/file/drive/library/settings/control symbols use the original sheet.
# Installer is the explicit procedural exception: an original Tramontana badge.
S=4;im=Image.new('RGBA',(256*S,256*S));draw=ImageDraw.Draw(im)
def xy(v):return tuple(round(a*S) for a in v)
draw.rounded_rectangle(xy((15,12,241,239)),radius=48*S,fill='#0c333e',outline='#91ddd8',width=3*S)
draw.rounded_rectangle(xy((25,22,231,229)),radius=39*S,fill='#165667')
draw.polygon([xy(q) for q in [(36,159),(88,75),(124,128),(153,91),(222,178)]],fill='#cde9e6')
draw.polygon([xy(q) for q in [(69,106),(88,75),(107,105),(92,100),(84,109)]],fill='#ffffff')
draw.rounded_rectangle(xy((41,182,215,211)),radius=10*S,fill='#092b39',outline='#72d5cf',width=2*S)
draw.line([xy((136,45)),xy((136,134))],fill='#fff9df',width=13*S)
draw.polygon([xy(q) for q in [(111,116),(136,143),(161,116)]],fill='#fff9df')
draw.ellipse(xy((191,191,199,199)),fill='#baf4d5')
im.resize((256,256),Image.Resampling.LANCZOS).save(D/'installer-tramontana.png',optimize=True)
mapping[38]='installer-tramontana.png'
SIZES=(16,24,32,48,64,128);header=bytearray(43*len(SIZES)*4);body=bytearray();seen={};legacy=(a/'symbols.rgba').read_bytes();base64=[]
for slot in range(43):
    name=mapping.get(slot)
    if name:
        path=D/name if slot==38 else source/name
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
