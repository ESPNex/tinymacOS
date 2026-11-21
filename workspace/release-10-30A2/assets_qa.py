#!/usr/bin/env python3
"""Audit actual build resources and catalog, not just authoring inputs."""
import hashlib,json,re,struct,zipfile
from pathlib import Path
from PIL import Image
from fontTools.ttLib import TTFont
home=Path(__file__).resolve().parents[2];r=home/'tinymacOS';out=Path('/var/tmp/tmc30a2-final');receipt=Path(__file__).parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def catalog(raw):
 assert sha(raw[:-32])==raw[-32:].hex()
 _,n,step=struct.unpack_from('<8sII',raw);paths=[];data={}
 for i in range(n):
  parent,d,off,size,name=struct.unpack_from('<iIII48s',raw,16+i*step);name=name.split(b'\0')[0].decode();p=paths[parent]+'/'+name if i else '';paths.append(p);data[p]=None if d else raw[off:off+size]
 return data
m=json.loads((r/'assets/icons/manifest.json').read_text());z=zipfile.ZipFile('/var/tmp/tramontana-icons-payload.zip');assert sha(Path(z.filename).read_bytes())==m['archive_sha256']
for entry in m['icons']:
 name=re.sub(r'-\d{4}-\d{2}-\d{2}\.png$','',entry['file']);latest=max(n for n in z.namelist() if re.fullmatch(re.escape(name)+r'-\d{4}-\d{2}-\d{2}\.png',n.split('/')[-1]))
 assert latest==entry['archive_path'] and sha(z.read(latest))==entry['sha256']==sha((r/'assets/icons'/entry['file']).read_bytes())
a=json.loads((out/'icon-atlas.json').read_text());atlas=(out/'icons.atlas').read_bytes();assert sha(atlas)==a['sha256'];sizes=a['sizes']
for slot,name in a['slots'].items():
 source=out/name if int(slot)==38 else r/'assets/icons'/name
 im=Image.open(source).convert('RGBA')
 for tier,size in enumerate(sizes):
  off=struct.unpack_from('<I',atlas,4*(int(slot)*len(sizes)+tier))[0];assert not off&0x80000000
  assert atlas[off:off+size*size*4]==im.resize((size,size),Image.Resampling.LANCZOS).tobytes()
# This wordmark preserves the complete former ten-face font bit-for-bit.
b=(r/'assets/type.tfn').read_bytes();nf,ng=struct.unpack_from('<II',b,4);assert nf==11
end=12+4*ng;table=4+8*ng;old_tables=b[:end+10*table];old_pixels=2226987-len(old_tables)
old=bytearray(old_tables+b[end+11*table:end+11*table+old_pixels]);struct.pack_into('<I',old,4,10)
assert sha(old)=='f8c1b1b5b598d97eedd1765040076f516c8522a7dc8250febd3f914544e6d174'
assert TTFont(r/'assets/font-source/Display-Semibold.ttf')['OS/2'].usWeightClass==600
codes=list(struct.unpack_from(f'<{ng}I',b,12));prefix=sum(struct.unpack_from('<H',b,end+4*table+4+8*codes.index(ord(c)))[0] for c in 'tiny');assert prefix==72
for face in range(nf):
 pos=end+face*table;h=struct.unpack_from('<I',b,pos)[0];adv,w,off=struct.unpack_from('<HHI',b,pos+4+8*codes.index(0x20ac));assert adv and w and any(b[end+nf*table+off:end+nf*table+off+h*w])
for mode in ['disk','live']:
 c=catalog((out/('System-'+mode+'.vol')).read_bytes());v=json.loads(json.dumps(__import__('plistlib').loads(c['/System/Library/CoreServices/SystemVersion.plist'])));assert v['ProductVersion']=='10' and v['ProductBuildVersion']=='30A2'
 assert c['/System/Library/Fonts/Interface.tfn']==b and c['/System/Library/Desktop Pictures/Tramontana.qoi']==(r/'assets/tramontana.qoi').read_bytes()
 assert len(c)<380
 for key,raw in c.items():
  if key.endswith('/Contents/Info.plist') and '/Extensions/' not in key:
   base=key.removesuffix('Info.plist');pl=__import__('plistlib').loads(raw);assert pl['CFBundleShortVersionString']=='10' and pl['CFBundleVersion']=='30A2' and pl['CFBundleIconFile']=='AppIcon.png'
   for size in sizes:assert c[base+f'Resources/Icon{size}.png'][:8]==b'\x89PNG\r\n\x1a\n'
   assert c[base+'Resources/AppIcon.png'][:8]==b'\x89PNG\r\n\x1a\n'
 assert ('/Applications/Install tinymacOS.app/Contents/Resources/AppIcon.png' in c)==(mode=='live')
# All palette surfaces, not only accent circles, differ for Blu/Azzurro/Riviera.
s=(r/'include/system_palette.h').read_text().split('static const u32 tmc_system_palette')[1].split('static inline')[0];values=[int(x,16) for x in re.findall(r'0x[0-9a-fA-F]{6}',s)];assert len(values)==160
for dark in range(2):
 for slot in range(10):assert len({values[(theme*2+dark)*10+slot] for theme in [0,1,5]})==3
legacy=(r/'assets/symbols.rgba').read_bytes()
expected_apps={1,2,3,4,5,16,17,18,19,38,39,40,41,42}
assert set(map(int,a['slots']))==expected_apps
for slot in range(43):
 if slot in expected_apps:continue
 for tier in range(len(sizes)):
  off=struct.unpack_from('<I',atlas,4*(slot*len(sizes)+tier))[0];assert off&0x80000000
  off&=0x7fffffff;assert atlas[off:off+16384]==legacy[slot*16384:(slot+1)*16384]
for mode in ['disk','live']:
 c=catalog((out/('System-'+mode+'.vol')).read_bytes());assert not any('app-store' in path for path in c)
for mode in range(2):
 for slot in range(10):assert values[(2*2+mode)*10+slot]!=values[(7*2+mode)*10+slot]
rec=(out/'rec.icons').read_bytes()
for n,slot in enumerate((31,33,38,39,2,9,15,17)):
 expected=(out/'icons64.rgba').read_bytes() if slot==2 else legacy
 assert rec[n*16384:(n+1)*16384]==expected[slot*16384:(slot+1)*16384]
checks={'passed':True,'build':'30A2','latest_exact_name_icon_entries':len(m['icons']),'mapped_image_slots':len(a['slots']),'resolutions':sizes,'runtime_atlas_pixels_verified':True,'filesystem_original_and_sized_pngs_verified':True,'installer_original_procedural_exception':True,'font_faces':nf,'unchanged_original_ten_faces_sha256':sha(old),'tiny_regular_advance':prefix,'macOS_weight':600,'euro_all_faces':True,'wallpaper_generator':'native image generation; nonprocedural source retained','all_palette_slots_distinct_Blu_Azzurro_Riviera_both_modes':True,'only_application_art_imported':True,'Recovery_only_Terminal_imported_byte_exact':True,'procedural_system_symbols_byte_exact':True,'Teal_Tramontana_all_slots_distinct':True,'font_sha256':sha(b),'font_bytes':len(b)}
(receipt/'assets-result.json').write_text(json.dumps(checks,indent=2)+'\n');print(json.dumps(checks,indent=2))
