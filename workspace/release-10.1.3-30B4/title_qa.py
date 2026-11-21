#!/usr/bin/env python3
"""Actual Recovery framebuffer: title centered in its own window, including after drag."""
import hashlib,json,os,struct,sys,tempfile,time
from pathlib import Path
import numpy as np
q=Path(__file__).parent;home=q.parents[1];sys.path.insert(0,str(home/'tinymacOS/tests'))
from retail_guest import Retail
from boot_recovery import RecoveryKey
iso=Path('/var/tmp/tmc-10.1.3/tinymacOS-10.1.3-x86_64.iso');work=Path(tempfile.mkdtemp(prefix='tmc1011-title-',dir='/var/tmp'));ev=work/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',OCR_ROOT='/var/tmp/tmc/tools/root',TMC_MEM='512',TMC_POINTER='tablet',TMC_BUILD='/var/tmp/tmc-10.1.3/build')
class Rec(RecoveryKey,Retail):initial='Scegli come proseguire.'
b=(home/'tinymacOS/assets/type.tfn').read_bytes();nf,ng=struct.unpack_from('<II',b,4);codes={c:i for i,c in enumerate(struct.unpack_from(f'<{ng}I',b,12))};start=12+ng*4;pix=start+nf*(4+ng*8)
def mask(t):
 p=start+2*(4+ng*8);ht=struct.unpack_from('<I',b,p)[0];gs=[struct.unpack_from('<HHI',b,p+4+8*codes[ord(c)])for c in t];out=np.zeros((ht,sum(g[0]for g in gs)+4),np.uint8);x=0
 for adv,w,o in gs:out[:,x:x+w]=np.maximum(out[:,x:x+w],np.frombuffer(b,np.uint8,ht*w,pix+o).reshape(ht,w));x+=adv
 return out,x
r={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(work),'checks':[]}
def check(g,x,y,title,name):
 im=g.snap();a=np.asarray(im);m,adv=mask(title);xx=x+(1012-adv)//2;yy=y+12;region=a[yy:yy+m.shape[0],xx:xx+m.shape[1]]
 strong=m==255;assert strong.any();err=np.abs(region[strong].astype(int)-np.array([46,47,54]));assert err.max()==0,(name,int(err.max()))
 v,u=np.nonzero(m);cx=xx+(int(u.min())+int(u.max()))/2;cy=yy+(int(v.min())+int(v.max()))/2
 assert abs(cx-(x+1011/2))<=1
 if title=='tinymacOS Recovery':assert abs(cy-(y+42/2))<=1
 im.save(ev/(name+'.png'));im.crop((x,y,x+1012,y+43)).save(ev/(name+'-title.png'))
 r['checks'].append({'title':title,'window':[x,y,1012,560],'font':2,'shared_text_y':12,'opaque_text_error':0,'ink_center':[cx,cy],'window_title_center':[x+1011/2,y+42/2]})
try:
 with Rec(iso,work,ev,'title',live=True) as g:
  g.visible('tinymacOS Recovery');g.move(60,80);time.sleep(.35);check(g,134,120,'tinymacOS Recovery','recovery-centered')
  g.drag(484,142,70,35);g.move(60,80);time.sleep(.35);check(g,204,155,'tinymacOS Recovery','recovery-dragged-centered')
  g.key('down');g.key('down');g.key('down');g.key('ret');g.visible('Shell di recupero locale');g.move(60,80);check(g,204,155,'Terminale','terminal-shared-title-baseline')
  r['passed']=True
finally:(q/'title-result.json').write_text(json.dumps(r,indent=2)+'\n')
print('PASS actual Recovery title: exact shared font/baseline, centered X/Y, remains centered after moving window; Terminal follows same geometry',flush=True)
