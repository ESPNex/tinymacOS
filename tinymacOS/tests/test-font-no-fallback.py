#!/usr/bin/env python3
"""Rebuild with a font-open allowlist; check ABI, provenance and terminal grid.
Host asset contract only, not installed-system or guest visual acceptance.
"""
from pathlib import Path
import hashlib, json, runpy, struct, sys, tempfile
from PIL import ImageFont
root=Path(__file__).resolve().parents[1]
family=root/'assets/font-source/tinymacOS'
allowed={str(p.resolve()) for p in family.iterdir() if p.is_file()}
mono=root/'assets/font-source/tinymacOSMono'
allowed|={str(p.resolve()) for p in mono.iterdir() if p.is_file()}
original=ImageFont.truetype
opened=[]
def strict(font,*args,**kwargs):
    p=str(Path(font).resolve())
    assert p in allowed, f'Unexpected font source: {p}'
    opened.append(p)
    return original(font,*args,**kwargs)
ImageFont.truetype=strict
with tempfile.TemporaryDirectory(prefix='tmc-font-only-',dir='/var/tmp') as tmp:
    output=Path(tmp)/'interface.tfn'
    sys.argv=[str(root/'tools/interface-font.py'),str(family),str(output),'--mono-family',str(mono),'--panic-output',str(Path(tmp)/'panicfont.inc')]
    runpy.run_path(sys.argv[0],run_name='__main__')
    assert (Path(tmp)/'panicfont.inc').read_bytes()==(root/'assets/panicfont.inc').read_bytes()
    data=output.read_bytes();manifest=json.loads(output.with_suffix('.json').read_text())
assert data==(root/'assets/type.tfn').read_bytes(),'Production atlas not reproducible'
lock=json.loads((root/'assets/interface-font-lock.json').read_text())
assert hashlib.sha256(data).hexdigest()==lock['sha256'] and len(data)==lock['bytes']
assert set(opened)==allowed and len(opened)==12
assert manifest['faces'][5]['file']=='tinymacOSMonoRegular.otf'
assert not manifest['external_font_fallback'] and not manifest['inherited_monospace']
assert all(not f['fallback_glyphs'] for f in manifest['faces'])
assert data[:4]==b'TFN2'
nf,ng=struct.unpack_from('<II',data,4);assert (nf,ng)==(11,207)
codes=struct.unpack_from(f'<{ng}I',data,12);end=12+4*ng;step=4+8*ng;bits=end+nf*step
for face,f in enumerate(manifest['faces']):
    height=struct.unpack_from('<I',data,end+face*step)[0];assert height==f['height']
    for j,code in enumerate(codes):
        advance,width,offset=struct.unpack_from('<HHI',data,end+face*step+4+j*8)
        assert 0<width<200 and 0<advance<200 and bits+offset+width*height<=len(data)
        if face==5:assert advance==width==8
        if code in (0x2318,0x2315,0x232b):assert any(data[bits+offset:bits+offset+width*height])
assert len(data)<3*1024*1024
for name in ('art.py','interface-font.py'):
    text=(root/'tools'/name).read_text().lower()
    assert 'dejavu' not in text
    if name=='art.py':assert "(d/'type.tfn').write_bytes" not in text
assert not (root/'tools/add-italian-glyphs.py').exists()
assert not (root/'tools/tramontana-font.py').exists()
print('PASS deterministic TFN2: only 5 supplied OTFs opened, all 11 faces rebuilt, 207 glyphs each; no inherited mono, fallback or host font; 8px terminal grid and original UI symbols; lock matches. NOT guest/media validation.')
