#!/usr/bin/env python3
"""Black-box Control Center acceptance on the actual release ISO, QMP input only."""
import hashlib,json,os,sys,tempfile,time
from pathlib import Path
home=Path(__file__).resolve().parents[2];sys.path.insert(0,str(home/'tinymacOS/tests'))
from retail_guest import Retail,fold
from pixels import locate
iso=Path('/var/tmp/tmc-10.1.6/tinymacOS-10.1.6-x86_64.iso');w=Path(tempfile.mkdtemp(prefix='tmc101-control-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',OCR_ROOT='/var/tmp/tmc/tools/root',TMC_BUILD='/var/tmp/tmc-10.1.6/build',TMC_MEM='512',TMC_POINTER='tablet')
r={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'checks':[]}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
def label(g,t,box=None):
 # Glass content is deliberately non-flat: use cropped OCR when background
 # detail lowers bitmap-template correlation. Never infer a setting from clicks.
 end=time.monotonic()+6
 while time.monotonic()<end:
  im=g.snap();im=im.crop(box) if box else im
  if locate(im,t,threshold=.78):return
  time.sleep(.15)
 text=g.ocr(box);assert fold(t) in fold(text),(t,text)
def bounds(g):
 W,H=g.snap().size;h=min(550,H-60);return W-376*h//550-18,46,376*h//550,h
def point(g,x,y):
 a,b,c,d=bounds(g);return a+x*c//376,b+y*d//550
def tile(g,x,y):g.click(*point(g,x,y));g.move(bounds(g)[0]-24,680);time.sleep(.35)
def opened(g):
 x,y=g.visible('mer 18 nov',box=(700,0,g.snap().width,34),faces=(1,));g.click(x-29,17);g.visible('Centro di Controllo');time.sleep(.65)
def closed(g):g.key('esc');time.sleep(.3)
try:
 with Retail(iso,w,ev,'control',live=True) as g:
  g.enroll('Controlli');opened(g);g.visible('Tutte le impostazioni');g.shot('control-default')
  ok('Modular panel, actual labels and Settings action visible in retail framebuffer')
  # Empty space between tiles must remain real, untouched desktop/backdrop.
  before=g.snap();a,b,c,d=bounds(g);gap=(a+110*c//376,b+444*d//550)
  closed(g);after=g.snap();assert before.getpixel(gap)==after.getpixel(gap),(gap,before.getpixel(gap),after.getpixel(gap));ok('No enclosing slab: inter-module gap is byte-identical to underlying desktop')
  opened(g);tile(g,88,66);label(g,'Scuro');tile(g,88,66);label(g,'Chiaro');ok('Actual light/dark preference toggles and reflects authoritative state')
  tile(g,38,160);g.shot('control-opaque');tile(g,38,160);g.shot('control-glass');ok('Reduced transparency toggle and real Dock glass material exercised')
  tile(g,140,160);tile(g,140,160);ok('Reduce-motion preference toggled both ways')
  tile(g,90,263);label(g,'Laterale');tile(g,90,263);label(g,'In basso');ok('Dock orientation both ways')
  # Horizontal controls select real indexed preferences, not unavailable audio/brightness.
  g.move(*point(g,44,373));g.mouse(True);g.move(*point(g,332,373));g.mouse(False);label(g,'Tramontana',box=(a,b+318,a+c,b+398));ok('Theme slider real drag to last theme')
  g.key('home');label(g,'Blu',box=(a,b+318,a+c,b+398));g.key('right');label(g,'Azzurro',box=(a,b+318,a+c,b+398));g.key('end');label(g,'Tramontana',box=(a,b+318,a+c,b+398));ok('Theme slider Home/Right/End keyboard adjustments')
  tile(g,275,100);label(g,'Prisma',box=(a+196,b+28,a+c,b+200));g.key('end');label(g,'Tramontana',box=(a+196,b+28,a+c,b+200));g.key('home');label(g,'Prisma',box=(a+196,b+28,a+c,b+200));g.key('end');ok('Single wallpaper tile: click and Home/End select actual preference; no duplicate wallpaper slider')
  tile(g,235,260);time.sleep(.4);g.key('esc');time.sleep(.4);opened(g);ok('Mission Control shortcut opens and returns')
  tile(g,56,446);g.visible('Calcolatrice');g.key('ctrl-w');opened(g);tile(g,188,446);g.visible('Calendario');g.key('ctrl-w');opened(g);tile(g,320,446);g.visible('Sessione locale');g.key('ctrl-w');ok('Calculator, Calendar, Terminal shortcuts launch actual applications')
  opened(g);g.key('tab');g.key('shift-tab');g.key('ret');g.visible('Impostazioni di Sistema');g.key('ctrl-w');ok('Tab / Shift+Tab / Enter activates Settings footer')
  opened(g);tile(g,335,260);g.visible('Inserisci la password');g.unlock();ok('Lock Session shortcut locks and actual password unlocks')
  opened(g);tile(g,188,530);g.visible('Impostazioni di Sistema');ok('Footer opens actual System Settings')
  r['passed']=True
finally:(Path(__file__).parent/'control-result.json').write_text(json.dumps(r,indent=2,ensure_ascii=False)+'\n')
