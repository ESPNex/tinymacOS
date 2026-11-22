#!/usr/bin/env python3
"""10.1.6 acceptance: imported PNG bytes, actual ISO payloads and rendered icon pixels.
No mocked interface or inferred physical compatibility. Screenshots are real QEMU frames.
"""
import hashlib,json,os,struct,sys,tempfile,time
from pathlib import Path
import numpy as np,cv2
from PIL import Image
q=Path(__file__).resolve().parent;home=q.parents[1];root=home/'tinymacOS';b=Path('/var/tmp/tmc-10.1.6/build');iso=b.parent/'tinymacOS-10.1.6-x86_64.iso'
sys.path.insert(0,str(root/'tests'))
from retail_guest import Retail
from boot_recovery import RecoveryKey
from pixels import locate
cv2.setNumThreads(1)
w=Path(tempfile.mkdtemp(prefix='tmc1016-ui-icons-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',OCR_ROOT='/var/tmp/tmc/tools/root',TMC_BUILD=str(b),TMC_POINTER='tablet',TMC_MEM='512')
def sha(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
r={'passed':False,'iso_sha256':sha(iso),'work':str(w),'checks':[],'physical_hardware_tested':False}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
def catalog(raw):
 data=raw[:-32];assert hashlib.sha256(data).digest()==raw[-32:];_,n,step=struct.unpack_from('<8sII',data);paths=[];out={}
 for j in range(n):
  parent,d,off,size,name=struct.unpack_from('<iIII48s',data,16+j*step);name=name.split(b'\0')[0].decode();p=paths[parent]+'/'+name if j else '';paths.append(p);out[p]=None if d else data[off:off+size]
 return out
atlas=(b/'icons.atlas').read_bytes();flat=(b/'icons64.rgba').read_bytes();rec=(b/'rec.icons').read_bytes()
def tier(slot,size):
 sizes=[16,24,32,48,64,128];s=next(n for n in sizes if n>=size);off=struct.unpack_from('<I',atlas,(slot*6+sizes.index(s))*4)[0];assert not off&0x80000000
 a=np.frombuffer(atlas,np.uint8,count=s*s*4,offset=off).reshape(s,s,4);idx=np.arange(size)*s//size;return a[idx[:,None],idx[None,:]]
def match(g,sprite,name):
 a=np.asarray(g.snap());mask=np.uint8(sprite[:,:,3]==255);assert mask.sum()>30
 score=cv2.matchTemplate(a,sprite[:,:,:3].copy(),cv2.TM_SQDIFF,mask=mask)
 _,_,pos,_=cv2.minMaxLoc(score);x,y=pos;region=a[y:y+len(sprite),x:x+sprite.shape[1]]
 # Exact native integer sampling, alpha==255 so compositing cannot hide a mismatch.
 error=np.abs(region[mask>0].astype(int)-sprite[:,:,:3][mask>0].astype(int));assert error.max()==0,(name,pos,int(error.max()))
 r.setdefault('rendered_icons',[]).append({'name':name,'position':[x,y],'size':len(sprite),'opaque_pixels':int(mask.sum()),'max_channel_error':int(error.max())});g.shot(name)
try:
 m=json.loads((root/'assets/icons/manifest.json').read_text());entry=next(x for x in m['icons'] if x['slot']==13);assert entry['file']=='app-store-2026-09-23.png'
 supplied=root/'assets/icons'/entry['file'];assert sha(supplied)==entry['sha256'];assert max(p.name for p in supplied.parent.glob('app-store-????-??-??.png'))==supplied.name
 assert bytes(Image.open(supplied).convert('RGBA').resize((64,64),Image.Resampling.LANCZOS).tobytes())==flat[13*16384:14*16384]
 assert rec[3*16384:4*16384]==flat[39*16384:40*16384]
 # Remaining system artwork remains the pre-existing procedural sheet.
 legacy=(root/'assets/symbols.rgba').read_bytes()
 for slot in [6,8,9,10,11,12,14,15,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37]:assert flat[slot*16384:(slot+1)*16384]==legacy[slot*16384:(slot+1)*16384],slot
 ok('Latest locally supplied exact-name App Store PNG in slot 13; Recovery Disk Utility byte-identical to OS slot 39; other filesystem symbols preserved')
 identity=json.loads((q/'identity-result.json').read_text());assert identity['passed'] and identity['iso_sha256']==r['iso_sha256'];media=Path(identity['evidence_external']).parent/'media'
 assert rec in (media/'recovery.efi').read_bytes()
 for name in ['opt.fat.vol','disk.esp.vol']:
  c=catalog((media/name).read_bytes());base='/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/'
  assert c[base+supplied.name]==supplied.read_bytes();assert c[base+'disk-utility-2026-09-23.png']==(root/'assets/icons/disk-utility-2026-09-23.png').read_bytes()
  assert atlas in c['/System/Library/CoreServices/WindowServer']
  assert c['/Applications/Utilities/Disk Utility.app/Contents/Resources/AppIcon.png']==(root/'assets/icons/disk-utility-2026-09-23.png').read_bytes()
 ok('Both actual ISO live/installable catalogs contain exact original PNGs and linked atlas; actual Recovery EFI contains matching imported icon subset')
 shell=(root/'desktop/shell.c').read_text();finder=(root/'userspace/finder.c').read_text();settings=(root/'userspace/settings.c').read_text()
 for text in ['Cambia sfondo','Cambia tema','Cambia aspetto']:assert text not in shell and text.encode() not in (b/'desktop.elf').read_bytes()
 assert 'bootpath(cwd) ? "Sistema" : "Finder"' not in finder and '"Sul mio computer" : "Sessione live"' not in finder
 assert 'Text(x, 14, "Impostazioni di Sistema"' not in settings and 'win(860, 540, "Impostazioni di Sistema")' in settings
 ok('Redundant desktop/View appearance commands absent from source and linked desktop; repeated Finder/Settings headers removed without removing appearance controls')
 with Retail(iso,w,ev,'ui',live=True) as g:
  g.enroll('Interfaccia');g.move(1190,680);match(g,tier(13,19),'finder-applications-sidebar')
  # Select the root filesystem location: exposes /Applications as a grid item.
  g.key('f2');g.visible('Sessione locale');g.cmd('open /');g.visible('tinymacOS Live');g.move(1190,680);time.sleep(.5)
  match(g,tier(13,54),'finder-applications-root')
  # Empty desktop context menu, away from icons, widget and all windows.
  g.move(1240,510);g.req('input-send-event',events=[{'type':'btn','data':{'button':'right','down':True}}]);time.sleep(.12);g.req('input-send-event',events=[{'type':'btn','data':{'button':'right','down':False}}]);g.visible('Nuova cartella');g.shot('desktop-context-deduplicated')
  image=g.snap()
  for text in ['Cambia sfondo','Cambia tema']:assert not locate(image,text,threshold=.92)
  g.key('esc')
 ok('Actual Finder sidebar and /Applications root icon match supplied App Store artwork pixel-for-pixel; actual desktop context menu no longer repeats appearance actions')
 class Rec(RecoveryKey,Retail):initial='Scegli come proseguire.'
 # Utility Disco requires a genuine installed Tramontana volume, not a blank disk.
 import shutil
 installed=json.loads((q/'bios-installer-result.json').read_text());assert installed['passed'] and installed['iso_sha256']==r['iso_sha256']
 (w/'target').mkdir();shutil.copyfile(Path(installed['external_work'])/'target/system.raw',w/'target/system.raw')
 with Rec(iso,w,ev,'recovery',live=False,disk='target') as g:
  g.move(60,80);a=np.frombuffer(flat[39*16384:40*16384],np.uint8).reshape(64,64,4);idx=np.arange(39)*64//39;match(g,a[idx[:,None],idx[None,:]],'recovery-imported-disk-utility')
  g.key('down');g.key('down');g.key('ret');g.visible('Seleziona un disco');g.key('ret');g.visible('Gestisci il volume selezionato.');time.sleep(.35);g.shot('recovery-disk-utility-open')
 ok('Actual Recovery home renders the same imported Disk Utility artwork and opens Utility Disco')
 r['passed']=True
finally:(q/'ui-icons-result.json').write_text(json.dumps(r,indent=2,ensure_ascii=False)+'\n')
