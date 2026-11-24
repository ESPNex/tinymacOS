"""Black-box login-date and whole-sheet glass regression. Requires TMC_FONT, TMC_STAGE_ISO and TMC_BUILD."""
from pathlib import Path
import os,sys,time,json,hashlib
import numpy as np
from PIL import Image
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tests'))
from retail_guest import Retail
import pixels,struct
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font
pixels.nf,pixels.ng=struct.unpack_from('<II',font,4)
pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))}
pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
b=Path(os.environ.get('TMC_QA_DIR','/var/tmp/tmc-10.2'));b.mkdir(exist_ok=True,parents=True);ev=b/'screenshots';ev.mkdir(exist_ok=True);iso=Path(os.environ['TMC_STAGE_ISO'])
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD=os.environ['TMC_BUILD'],TMC_MEM='512',TMC_POINTER='tablet',PYTHONDONTWRITEBYTECODE='1')
r={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'physical_hardware_tested':False,'checks':[]}
def ok(t):r['checks'].append(t);print('PASS',t,flush=True)
def sorigin(g):
 x,y=g.visible('Cerca',faces=(0,));return x-20,y-21
def torigin(g):
 x,y=g.visible('Testo semplice',faces=(0,));return x-641,y-18
try:
 with Retail(iso,b,ev,'material',live=True) as g:
  im=g.snap();im.save(ev/'06-login-data.png');crop=im.crop((350,65,930,110));hits=[]
  for word in ['Mercoledì','18','novembre']:
   match=pixels.locate(crop,word,faces=(8,),threshold=.96);assert match,word
   _,cols=np.nonzero(pixels.word(word,8));hits.append((match[1][0]+int(cols.min()),int(cols.max()-cols.min()+1)))
  gaps=[hits[i+1][0]-sum(hits[i]) for i in range(2)];assert min(gaps)>=6,gaps
  r['login_date_visible_gaps_px']=gaps;ok('Login date has two visible word gaps of at least six pixels on actual framebuffer')
  g.enroll('Vetro');g.key('f1');g.visible('Preferiti');g.key('ctrl-w');time.sleep(.4)
  imgs=[]
  for j,wall in enumerate([0,7]):
   g.app('f4','Impostazioni di Sistema');X,Y=sorigin(g);g.click(X+80,Y+60);g.visible('SFONDO');g.click(X+270+(wall%7)*83,Y+320+(wall//7)*50);time.sleep(.5);g.key('ctrl-w');time.sleep(.3)
   g.app('f3','Testo semplice');TX,TY=torigin(g);g.key('ctrl-s');g.visible('Salva documento');g.move(1180,690);time.sleep(.6);img=np.asarray(g.snap(),dtype=np.int16);imgs.append(img);g.snap().save(ev/f'07-vetro-superficie-sfondo-{j}.png');g.key('esc')
  # Interior patches deliberately avoid text, fields, sidebar and button chrome.
  patches=[(280,32,80,8),(350,220,90,45),(400,410,80,12)]
  errors=[]
  for x,y,w,h in patches:
   a=imgs[0][TY+y:TY+y+h,TX+x:TX+x+w];c=imgs[1][TY+y:TY+y+h,TX+x:TX+x+w];errors.append(float(np.abs(a-c).mean()))
  r['sheet_wallpaper_response_rgb_mean']=errors;assert min(errors)>2,errors
  ok('Identical sheet changes with the actual wallpaper across header, center and footer, not only its rim')
  r['passed']=True
finally:(b/'material-result.json').write_text(json.dumps(r,indent=2)+'\n')
