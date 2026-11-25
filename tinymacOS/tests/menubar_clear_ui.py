"""Shipping ISO pixels: transparent desktop menubar, FileVault without Back.
Recovery opacity is covered separately by native_recovery.py and the shader test.
"""
from pathlib import Path
import os,time,json,struct,hashlib,tempfile
import numpy as np
from PIL import Image
from retail_guest import Retail
import pixels
root=Path(__file__).resolve().parents[1];base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO'])
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
w=Path(tempfile.mkdtemp(prefix='tmc-clear-menubar-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(OCR_ROOT=os.environ['TOOLROOT'],TMC_MEM='512',TMC_POINTER='tablet')
r={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'checks':[],'bar_pixel_max_errors':[],'physical_hardware_tested':False}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
def check(g,wall,dark):
 expected=np.asarray(Image.open(root/'assets'/f'{wall}.qoi').convert('RGB'),dtype=np.int32)
 if dark:expected=(expected*192+np.asarray([11,18,37])*64)>>8
 expected=expected[3:30,700:950]
 end=time.monotonic()+20
 while True:
  im=np.asarray(g.snap(),dtype=np.int32);assert im.shape==(800,1280,3)
  err=int(np.abs(im[3:30,700:950]-expected).max())
  if not err:break
  if time.monotonic()>end:raise AssertionError(('menubar does not equal actual wallpaper pixels',wall,dark,err))
  time.sleep(.2)
 r['bar_pixel_max_errors'].append(err)
try:
 with Retail(iso,w,ev,'clear',live=True) as g:
  g.enroll();g.key('f4');g.visible('Impostazioni di Sistema');x,y=g.visible('Cerca',faces=(0,));X,Y=x-20,y-21
  # Select actual wallpaper assets, not colors sampled from the rendered bar.
  g.click(X+282,Y+321);g.move(1180,650);check(g,'prisma',False);g.shot('menubar-light-prisma')
  g.click(X+697,Y+371);g.move(1180,650);check(g,'tramontana',False);g.shot('menubar-light-tramontana')
  g.click(X+682,Y+130);g.move(1180,650);check(g,'tramontana',True);g.shot('menubar-dark')
  g.click(X+804,Y+450);g.move(1180,650);check(g,'tramontana',True);g.shot('menubar-dark-opaque-windows')
  ok('Desktop menubar has zero RGB error against decoded wallpaper pixels on two wallpapers, light/dark and with opaque windows; no tint or blur overlay')
  x,y=g.visible('Sicurezza e privacy',faces=(0,));g.click(x+45,y+7);x,y=g.visible('FileVault…');g.click(x+45,y+7);g.visible('FileVault',faces=(3,));g.visible(('Disattivo','Volume non disponibile'))
  assert pixels.locate(g.snap(),'Indietro',threshold=.98) is None
  g.click(X+768,Y+46);g.visible('FileVault',faces=(3,));g.visible(('Disattivo','Volume non disponibile'));g.shot('filevault-no-back')
  ok('FileVault has no Back text/button; clicking its former hit area does not leave the section')
  x,y=g.visible('Sicurezza e privacy',box=(X,Y+40,X+208,Y+230),faces=(0,));g.click(x+40,y+7);g.visible('ACCOUNT')
  ok('Settings sidebar still returns to Privacy normally without a Back button')
 r['passed']=True
finally:(base/'menubar-clear-result.json').write_text(json.dumps(r,indent=2)+'\n')
