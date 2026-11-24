#!/usr/bin/env python3
"""Rasterize an explicitly supplied upright family to the fixed TFN2 UI ABI.
No font download, synthetic weight, metadata rewriting or license inference.
The supplied family stays external; the standard build asset is not overwritten.
"""
from pathlib import Path
import argparse, hashlib, json, struct
from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('family', type=Path, help='Directory: REGULAR.OTF, MEDIUM.OTF, SEMIBOLD.otf, BOLD.OTF')
p.add_argument('output', type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
raw = (root/'assets/type.tfn').read_bytes()
nf, ng = struct.unpack_from('<II', raw, 4)
assert nf == 11
codes = struct.unpack_from(f'<{ng}I', raw, 12)
end = 12+4*ng
step = 4+8*ng
bits = end+nf*step
names = {400:'REGULAR.OTF', 500:'MEDIUM.OTF', 600:'SEMIBOLD.otf', 700:'BOLD.OTF'}
fonts = {}
for weight, name in names.items():
    file = a.family/name
    f = TTFont(file)
    if f['OS/2'].usWeightClass != weight or f['post'].italicAngle != 0:
        raise SystemExit(f'{name}: expected real upright weight {weight}')
    fonts[weight] = (file, f.getBestCmap())
# Fixed heights match all native clients and Recovery; monospace stays unchanged.
faces = [(400,12,20),(400,14,22),(700,14,22),(600,25,33),(400,38,46),
         (None,13,21),(500,10,18),(400,54,62),(400,17,25),(700,86,94),(600,38,48)]
head = bytearray(raw[:end])
pixels = bytearray()
manifest = []
for face, (weight, size, height) in enumerate(faces):
    head.extend(struct.pack('<I', height))
    if weight is None:
        at = end+face*step
        assert struct.unpack_from('<I', raw, at)[0] == height
        for j in range(ng):
            advance, width, offset = struct.unpack_from('<HHI', raw, at+4+j*8)
            head.extend(struct.pack('<HHI', advance, width, len(pixels)))
            pixels.extend(raw[bits+offset:bits+offset+width*height])
        manifest.append({'face':face,'family':'DejaVu Sans Mono','weight':400})
        continue
    file, cmap = fonts[weight]
    font = ImageFont.truetype(str(file), size)
    fallback = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', size)
    missing = []
    for code in codes:
        selected = font if code in cmap else fallback
        if code not in cmap:
            missing.append(hex(code))
        ch = chr(code)
        box = selected.getbbox(ch)
        left = min(0, box[0])
        advance = round(selected.getlength(ch))
        width = max(advance, box[2])-left+1
        image = Image.new('L', (width,height))
        ImageDraw.Draw(image).text((-left,0), ch, font=selected, fill=255)
        head.extend(struct.pack('<HHI', advance,width,len(pixels)))
        pixels.extend(image.tobytes())
    manifest.append({'face':face,'family':'tinymacOS font (user supplied)',
                     'file':file.name,'weight':weight,'italic':False,'pixels':size,
                     'height':height,'fallback_glyphs':missing,
                     'source_sha256':hashlib.sha256(file.read_bytes()).hexdigest()})
data = head+pixels
if len(data) > 3*1024*1024:
    raise SystemExit('Font exceeds desktop buffer')
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_bytes(data)
record = {'format':'TFN2','faces':manifest,'glyph_count':ng,'bytes':len(data),
          'sha256':hashlib.sha256(data).hexdigest(),
          'provenance':'Supplied by user; redistribution rights not independently verified',
          'synthetic_weights':False,'inherited_monospace':True}
a.output.with_suffix('.json').write_text(json.dumps(record,indent=2)+'\n')
print(f'TFN2: {len(faces)} faces, {ng} glyphs, {len(data)} bytes; upright 400/500/600/700')
