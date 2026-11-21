#!/usr/bin/env python3
"""Real retail framebuffer checks for Tramontana appearance and icon tiers."""
import hashlib,json,os,re,struct,sys,tempfile,time
from pathlib import Path
import cv2,numpy as np
from PIL import Image
home=Path(__file__).resolve().parents[2];sys.path.insert(0,str(home/'tinymacOS/tests'))
from retail_guest import Retail
from pixels import locate,word,b,start,ng,codes,pix
from boot_recovery import RecoveryKey
iso=Path('/var/tmp/tmc30a2-final/tinymacOS-10-x86_64.iso');w=Path(tempfile.mkdtemp(prefix='tmc10-appearance-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',OCR_ROOT='/var/tmp/tmc/tools/root',TMC_BUILD=str(iso.parent),TMC_MEM='512',TMC_POINTER='tablet')
record={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'checks':[]}
def rawword(text,f):
 p=start+f*(4+ng*8);h=struct.unpack_from('<I',b,p)[0];gs=[struct.unpack_from('<HHI',b,p+4+8*codes[ord(c)]) for c in text];im=np.zeros((h,sum(g[0] for g in gs)+8),np.uint8);x=0
 for adv,width,off in gs:im[:,x:x+width]=np.maximum(im[:,x:x+width],np.frombuffer(b,np.uint8,count=h*width,offset=pix+off).reshape(h,width));x+=adv
 return im
def origin(g,text,f):
 x,y=g.visible(text,faces=(f,));v,u=np.nonzero(rawword(text,f));return x-max(0,int(u.min())-1),y-max(0,int(v.min())-1)
def iconmatch(im,slot,size):
 a=json.loads((iso.parent/'icon-atlas.json').read_text());tier=next((n for n in a['sizes'] if n>=size),128);name=a['slots'][str(slot)]
 src=Image.open(iso.parent/'icon-png'/name.removesuffix('.png')/f'Icon{tier}.png').convert('RGBA');p=np.asarray(src)[np.arange(size)*tier//size][:,np.arange(size)*tier//size];mask=np.repeat((p[:,:,3]>250)[:,:,None],3,axis=2).astype(np.uint8)*255
 score=cv2.matchTemplate(np.asarray(im),p[:,:,:3],cv2.TM_SQDIFF_NORMED,mask=mask);mn,_,xy,_=cv2.minMaxLoc(score);assert mn<.001,(slot,size,mn,xy);return {'slot':slot,'display_size':size,'source_tier':tier,'normalized_squared_error':mn,'location':xy}
def systemmatch(im,slot,size):
 legacy=np.frombuffer((home/'tinymacOS/assets/symbols.rgba').read_bytes(),np.uint8).reshape(-1,64,64,4)[slot]
 p=legacy[np.arange(size)*64//size][:,np.arange(size)*64//size];mask=np.repeat((p[:,:,3]>250)[:,:,None],3,axis=2).astype(np.uint8)*255
 mn,_,xy,_=cv2.minMaxLoc(cv2.matchTemplate(np.asarray(im),p[:,:,:3],cv2.TM_SQDIFF_NORMED,mask=mask));assert mn<.002,(slot,size,mn,xy)
 return {'slot':slot,'size':size,'error':mn,'location':xy}
try:
 with Retail(iso,w,ev,'appearance',live=True) as g:
  g.enroll('Tramontana');g.move(1050,610);time.sleep(2);im=g.snap();g.shot('default-desktop')
  # Actual desktop wallpaper pixel away from all windows, menu, Dock and widgets.
  wall=Image.open(home/'tinymacOS/assets/tramontana.qoi').convert('RGB');assert im.getpixel((20,400))==wall.getpixel((20,400)),(im.getpixel((20,400)),wall.getpixel((20,400)))
  record['wallpaper_actual_pixel_match']=True
  record['system_icons']=[systemmatch(im,20,49),systemmatch(im,23,54),systemmatch(im,7,54)];record['dock_icon']=iconmatch(im,1,49);record['desktop_installer_icon']=iconmatch(im,38,50)
  g.key('f2');g.visible('Sessione locale');g.cmd('open -a "About This Computer"');g.visible('Versione 10');g.visible('tiny',faces=(4,));g.visible('macOS',faces=(10,));g.shot('about-sem  ibold'.replace('  ',''));g.key('ctrl-w');g.key('ctrl-w')
  record['split_wordmark_visible_regular4_semibold10']=True
  g.key('f9');g.visible('Vento del nord');g.visible('Tramontana');g.shot('gallery-tramontana');g.key('left');g.visible('Brezza');g.key('right');g.visible('Vento del nord');g.key('ctrl-w');record['gallery_thirteenth_entry_and_keyboard_navigation']=True
  g.key('f4');g.visible('Impostazioni di Sistema');tx,ty=origin(g,'Impostazioni di Sistema',0);cx,cy=tx-226,ty-14
  assert 0<=cx<400 and 34<cy<300,(cx,cy)
  record['system_icons'].append(systemmatch(g.snap(),28,18))
  g.visible('Tramontana');raw=(home/'tinymacOS/include/system_palette.h').read_text().split('static const u32 tmc_system_palette')[1].split('static inline')[0];palette=[int(v,16) for v in re.findall(r'0x[0-9a-fA-F]{6}',raw)]
  for dark in [0,1]:
   g.click(cx+226+16+142+dark*300,cy+130);time.sleep(.4)
   for theme,name in [(0,'Blu'),(1,'Azzurro'),(2,'Teal'),(5,'Riviera'),(7,'Tramontana')]:
    g.click(cx+226+33+theme*40,cy+243);g.visible(name,faces=(2,));expectedwall={0:'Prisma',1:'Brezza',2:'Prisma glaciale',5:'Riviera',7:'Tramontana'}[theme];g.visible(expectedwall,faces=(2,),box=(cx+226,cy+267,cx+840,cy+292));g.move(1050,610);time.sleep(.5);im=g.snap();rgb=palette[(theme*2+dark)*10+8];expected=((rgb>>16)&255,(rgb>>8)&255,rgb&255);assert im.getpixel((cx+215,cy+420))==expected,(name,dark,im.getpixel((cx+215,cy+420)),expected)
    im.crop((cx+208,cy+50,cx+860,cy+405)).save(ev/(name+('-dark' if dark else '-light')+'.png'),optimize=True)
    record['checks'].append({'theme':name,'dark':bool(dark),'actual_surface_rgb':expected,'associated_wallpaper':expectedwall,'full_frame_external':str(ev)})
  # The new 13th thumbnail is selectable and reports Tramontana by name.
  g.click(cx+226+18+5*83+35,cy+352+18);g.visible('Tramontana');g.shot('wallpaper-thirteenth');gx,gy=g.visible('Generali',faces=(0,));g.click(gx+20,gy+6);g.visible('tinymacOS 10 (30A2)');g.shot('settings-version-10');record['settings_internal_version_10_verified']=True;g.key('ctrl-w');g.click(1099,16);g.visible('Centro di Controllo');time.sleep(.6);im=g.snap();record['system_icons'] += [systemmatch(im,19,23),systemmatch(im,4,27)];g.shot('control-procedural');g.key('esc')
 class Rec(RecoveryKey,Retail):pass
 with Rec(iso,w,ev,'recovery-appearance',live=True) as g:
  im=g.snap();a=np.asarray(im);mask=np.array([int(v) for v in (home/'tinymacOS/assets/apple-menu.inc').read_text().replace('\n','').split(',') if v.strip()],np.uint8).reshape(24,24)
  gray=cv2.cvtColor(a[:34,:45],cv2.COLOR_RGB2GRAY);_,score,_,xy=cv2.minMaxLoc(cv2.matchTemplate(gray,mask,cv2.TM_CCOEFF_NORMED));assert score>.97 and xy==(12,3),(score,xy)
  # Compare ink extents, not text baselines or bounding boxes including leading.
  tx,ty=origin(g,'Recupero tinymacOS — Tastiera ITA',1);glyph=rawword('Recupero tinymacOS — Tastiera ITA',1);ys=np.nonzero(glyph>=64)[0];apple_y=np.nonzero(mask>=64)[0];difference=abs((ys.min()+ys.max())/2+ty-((apple_y.min()+apple_y.max())/2+3));assert difference<=1,difference
  g.visible('Recupero tinymacOS',faces=(2,),box=(80,80,1200,300));g.shot('recovery-smaller-title');im.crop((0,0,650,34)).save(ev/'recovery-menu-alignment.png',optimize=True)
  record['recovery']={'glyph_origin':xy,'apple_text_center_difference_px':float(difference),'smaller_title_face':2}
 record['passed']=True
finally:(Path(__file__).parent/'appearance-result.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps(record,indent=2))
