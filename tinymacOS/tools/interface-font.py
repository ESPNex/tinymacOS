#!/usr/bin/env python3
"""Build TFN2 using only the supplied upright family and original UI symbols.
No host fonts, inherited bitmap faces, fallback family or network access.
Missing unsupported characters abort conversion. Source licensing is not inferred.
"""
from pathlib import Path
import argparse, hashlib, json, struct
from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('family', type=Path, help='REGULAR.OTF, MEDIUM.OTF, SEMIBOLD.otf, BOLD.OTF')
p.add_argument('output', type=Path)
p.add_argument('--mono-family', type=Path, required=True, help='Directory containing tinymacOSMonoRegular.otf')
p.add_argument('--panic-output', type=Path, help='Also rebuild emergency ASCII 8x15 include from supplied Mono Regular')
a = p.parse_args()
codes = list(range(32,127))+list(range(160,256))+list(map(ord,'…←→↑↓⌘•−▸⌕✓’–—⌫€'))
assert len(codes) == len(set(codes)) == 207
names = {400:'REGULAR.OTF',500:'MEDIUM.OTF',600:'SEMIBOLD.otf',700:'BOLD.OTF'}
fonts = {}
for weight,name in names.items():
    file = a.family/name
    with TTFont(file) as f:
        if f['OS/2'].usWeightClass != weight or f['post'].italicAngle != 0:
            raise SystemExit(f'{name}: expected real upright weight {weight}')
        fonts[weight] = (file, f.getBestCmap())
mono_file=a.mono_family/'tinymacOSMonoRegular.otf'
with TTFont(mono_file) as f:
    mono_cmap=f.getBestCmap()
    if f['OS/2'].usWeightClass!=400 or f['post'].italicAngle!=0 or not f['post'].isFixedPitch:
        raise SystemExit('Expected real upright weight-400 monospace')
    advances={f['hmtx'][mono_cmap[c]][0] for c in range(32,127)}
    if len(advances)!=1:raise SystemExit('ASCII source advances are not fixed')
# Same ABI heights and face indexes as all native clients. Face 5 uses the
# supplied Mono Regular outlines fitted into the existing 8px terminal grid.
faces = [(400,12,20),(400,14,22),(700,14,22),(600,25,33),(400,38,46),
         (400,13,21),(500,10,18),(400,54,62),(400,17,25),(700,86,94),(600,38,48)]

def symbol(code,size,height):
    """Original geometric UI marks; not traced/copied from another font."""
    scale=4; width=max(5,round(size*.9)); im=Image.new('L',(width*scale,height*scale));d=ImageDraw.Draw(im)
    top=max(1,round(size*.2)); span=max(3,size*.7)
    def xy(x,y):return (round((1+x*(width-3))*scale),round((top+y*span)*scale))
    line=max(1,round(size*.075*scale))
    def path(points):d.line([xy(x,y) for x,y in points],fill=255,width=line,joint='curve')
    if code==0x2315: # search
        d.ellipse((*xy(.04,.02),*xy(.67,.65)),outline=255,width=line);path([(.60,.60),(1,1)])
    elif code==0x232b: # backspace
        path([(.3,.12),(1,.12),(1,.88),(.3,.88),(0,.5),(.3,.12)])
        path([(.48,.32),(.82,.68)]);path([(.48,.68),(.82,.32)])
    elif code==0x2318: # command, original four-loop geometry
        for x,y in ((0,0),(.62,0),(0,.62),(.62,.62)):
            d.rounded_rectangle((*xy(x,y),*xy(x+.38,y+.38)),radius=max(1,round(size*.09*scale)),outline=255,width=line)
        path([(.19,.38),(.19,.62)]);path([(.81,.38),(.81,.62)])
        path([(.38,.19),(.62,.19)]);path([(.38,.81),(.62,.81)])
    else:raise ValueError(hex(code))
    return im.resize((width,height),Image.Resampling.LANCZOS),width

head=bytearray(b'TFN2'+struct.pack('<II',len(faces),len(codes))+struct.pack(f'<{len(codes)}I',*codes))
pixels=bytearray();manifest=[]
for face,(weight,size,height) in enumerate(faces):
    file,cmap=(mono_file,mono_cmap) if face==5 else fonts[weight]
    font=ImageFont.truetype(str(file),size)
    head.extend(struct.pack('<I',height));custom=[];aliases=[]
    for code in codes:
        if code in (0x2318,0x2315,0x232b):
            image,advance=symbol(code,size,height);custom.append(hex(code))
        else:
            # Soft hyphen is explicitly represented by this family's hyphen.
            selected=0x2d if code==0xad else code
            if face==5:selected={0xa0:0x20,0xb2:ord('2'),0xb3:ord('3'),0xb9:ord('1')}.get(selected,selected)
            if selected not in cmap:raise SystemExit(f'{file.name}: missing U+{code:04X}; no fallback permitted')
            if code!=selected:aliases.append({'code':hex(code),'source':hex(selected)})
            ch=chr(selected);box=font.getbbox(ch);left=min(0,box[0]);advance=round(font.getlength(ch))
            width=max(advance,box[2])-left+1
            image=Image.new('L',(width,height));ImageDraw.Draw(image).text((-left,0),ch,font=font,fill=255)
            if face==5 and code in (0xb2,0xb3,0xb9):
                raised=image.resize((max(1,round(width*.65)),round(height*.65)),Image.Resampling.LANCZOS)
                image=Image.new('L',(width,height));image.paste(raised,(0,0))
        if face==5:
            # Keep fixed advance including narrow/wide letters and spaces.
            box=image.getbbox();cell=Image.new('L',(8,height))
            if box:
                glyph=image.crop((box[0],0,box[2],height))
                if glyph.width>8:glyph=glyph.resize((8,height),Image.Resampling.LANCZOS)
                cell.paste(glyph,((8-glyph.width)//2,0))
            image=cell;advance=8
        head.extend(struct.pack('<HHI',advance,image.width,len(pixels)));pixels.extend(image.tobytes())
    manifest.append({'face':face,'family':'tinymacOS Mono (user supplied)' if face==5 else 'tinymacOS (user supplied)','file':file.name,'weight':weight,
                     'pixels':size,'height':height,'monospace':face==5,'fixed_advance':8 if face==5 else None,
                     'original_project_symbols':custom,'same_family_aliases':aliases,'fallback_glyphs':[],
                     'source_sha256':hashlib.sha256(file.read_bytes()).hexdigest()})
data=head+pixels
if len(data)>3*1024*1024:raise SystemExit('Font exceeds desktop buffer')
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(data)
record={'format':'TFN2','faces':manifest,'glyph_count':len(codes),'bytes':len(data),
        'sha256':hashlib.sha256(data).hexdigest(),'provenance':'User-supplied OTFs; redistribution license not included in source ZIP',
        'synthetic_weights':False,'inherited_monospace':False,'external_font_fallback':False,
        'monospace_policy':'Supplied Mono Regular 13px fitted into 8px cells; no synthetic weights or inherited bitmaps',
        'symbols_policy':'Original geometric command/search/backspace; soft hyphen uses same-family hyphen; mono NBSP uses space, superscripts use scaled same-family digits'}
a.output.with_suffix('.json').write_text(json.dumps(record,indent=2)+'\n')
print(f'TFN2: {len(faces)} faces, {len(codes)} glyphs, {len(data)} bytes; no external font fallback or inherited face')

if a.panic_output:
    emergency=ImageFont.truetype(str(mono_file),12);rows=[]
    for code in range(32,127):
        ch=chr(code);box=emergency.getbbox(ch);left=min(0,box[0]);width=max(round(emergency.getlength(ch)),box[2])-left+1
        glyph=Image.new('L',(width,15));ImageDraw.Draw(glyph).text((-left,0),ch,font=emergency,fill=255)
        ink=glyph.getbbox();cell=Image.new('L',(8,15))
        if ink:
            g=glyph.crop((ink[0],0,ink[2],15))
            if g.width>8:g=g.resize((8,15),Image.Resampling.LANCZOS)
            cell.paste(g,((8-g.width)//2,0))
        rows.extend(sum((1<<x) for x in range(8) if cell.getpixel((x,y))>=96) for y in range(15))
    assert len(rows)==95*15
    a.panic_output.parent.mkdir(parents=True,exist_ok=True)
    a.panic_output.write_text(','.join(map(str,rows))+'\n')
    print('Emergency 8x15 ASCII rebuilt from supplied Mono Regular, no inherited glyphs')
