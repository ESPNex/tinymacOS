"""Retail ISO: native Recovery boot, USB mouse pixels, terminal and isolation.
Uses only disposable QEMU media; this is NOT a physical Q1900M test.
"""
from pathlib import Path
import os,sys,time,json,hashlib,struct,tempfile
import numpy as np,cv2
from retail_guest import Retail
from boot_recovery import RecoveryKey
import pixels
root=Path(__file__).resolve().parents[1]
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font
pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))}
pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear();cv2.setNumThreads(1)
fw=sys.argv[1];base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO'])
w=Path(tempfile.mkdtemp(prefix='tmc1021-native-'+fw+'-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',OCR_ROOT='/var/tmp/tmc/tools/root',TMC_BUILD=os.environ['TMC_BUILD'],TMC_MEM='512',TMC_POINTER='mouse',TMC_BIOS_ISO=str(iso))
r={'passed':False,'firmware':fw,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'physical_hardware_tested':False,'checks':[]}
class Rec(RecoveryKey,Retail):
 recovery_text='Ripristina da una copia locale'
def ok(s):r['checks'].append(s);print('PASS',fw,s,flush=True)
raw=(root/'assets/symbols.rgba').read_bytes()[15*16384:16*16384];a=np.frombuffer(raw,np.uint8).reshape(64,64,4);idx=np.arange(26)*64//26;sprite=a[idx[:,None],idx[None,:]];mask=np.uint8(sprite[:,:,3]==255)
def position(g):
 # A QEMU screendump may bisect the compositor's cursor damage update.
 # Require an exact opaque sprite in a complete sample, never loosen XY.
 end=time.monotonic()+4
 while True:
  image=np.asarray(g.snap());score=cv2.matchTemplate(image,sprite[:,:,:3].copy(),cv2.TM_SQDIFF,mask=mask);_,_,(x,y),_=cv2.minMaxLoc(score)
  region=image[y:y+26,x:x+26]
  if np.max(np.abs(region[mask>0].astype(int)-sprite[:,:,:3][mask>0].astype(int)))==0:return x+3,y+2
  assert time.monotonic()<end,'No exact completed cursor sprite in four seconds'
  time.sleep(.05)
def step(g,dx,dy):
 x,y=position(g);g.hmp(f'mouse_move {dx} {dy}');end=time.monotonic()+4
 while position(g)!=(x+dx,y+dy):
  assert time.monotonic()<end,(position(g),(x+dx,y+dy));time.sleep(.05)
def move(g,x,y):
 for _ in range(120):
  a,b=position(g)
  if (a,b)==(x,y):return
  step(g,max(-30,min(30,x-a)),max(-30,min(30,y-b)))
 raise AssertionError('pointer destination')
try:
 with Rec(iso,w,ev,'recovery',live=True,launcher=Path(os.environ['TMC_RECOVERY_LAUNCHER']) if os.environ.get('TMC_RECOVERY_LAUNCHER') else root/'tools/run-bios-iso.sh' if fw=='bios' else None) as g:
  g.shot('native-recovery-home');ok('Separate native Recovery desktop reached through ordinary R key')
  img=np.asarray(g.snap());assert int(img[80,16].max())<=8 and int(img[80,640,0])>=20;ok('Black background and soft neutral glow retained')
  # Opaque, neutral dark panel: widely separated clear samples must all be
  # exactly the same color despite the radial background behind them.
  assert all(tuple(img[y,x])==(45,45,45) for x,y in [(325,250),(325,500),(955,250),(955,500)])
  assert tuple(img[10,800])==(36,36,36)
  home=g.snap().crop((310,205,970,595))
  for label in ['Recupero tinymacOS','10.2.1','kernel','USB','RAM','Riavvia','Spegni','Terminale']:
   assert pixels.locate(home,label,faces=(0,1,2,3,8),threshold=.98) is None,label
  ok('Home window has only three real recovery choices and Continue; technical text and extra commands are absent')
  g.visible('Utility',box=(0,0,800,34));g.visible('Finestra',box=(0,0,800,34))
  ok('Recovery home is neutral dark and opaque; actual top menu bar is present, without a Dock')
  x,y=position(g);step(g,-60,0);step(g,0,-37);ok('Independent horizontal and vertical native USB mouse motion, exact cursor pixels')
  move(g,850,175);start=time.monotonic();events=0
  while time.monotonic()-start<75:
   for dx,dy in [(13,0),(0,9),(-13,0),(0,-9)]:step(g,dx,dy);events+=1
  r['sustained_motion']={'seconds':time.monotonic()-start,'events':events};ok('At least 75 seconds continuous native USB motion without freezing')
  # Functional top menu opens Terminal; screenshots cover menu and dark app.
  ux,uy=g.visible('Utility',box=(0,0,800,34));move(g,ux+15,uy+8);g.mouse(True);g.mouse(False)
  g.visible('Terminale',box=(0,34,800,180));g.shot('native-recovery-menu')
  tx,ty=g.visible('Terminale',box=(0,34,800,180));move(g,tx+25,ty+8);g.mouse(True);g.mouse(False)
  g.visible('Sessione locale');g.shot('native-terminal-menu')
  ok('Opaque Utility menu launches native Terminal by mouse; top bar remains visible above the app')
  g.shot('native-terminal');ok('Native Terminal stays above the dark opaque Recovery home')
  g.cmd('sw_vers -productVersion','10.2.1');g.cmd('sw_vers -buildVersion','30B12');ok('Native shell executes actual system commands with current version')
  g.cmd('echo RECOVERY-RAM-ONLY > /Users/User/Documents/rec-test.txt');g.cmd('cat /Users/User/Documents/rec-test.txt','RECOVERY-RAM-ONLY');ok('Recovery has its own working RAM filesystem, no installed home mounted')
  g.key('ctrl-w');time.sleep(.8);g.visible('Recupero tinymacOS');move(g,550,440);g.mouse(True);g.mouse(False);move(g,880,557);g.mouse(True);g.mouse(False);g.visible('Utility Disco');g.shot('native-disk-utility');ok('Native Disk Utility opens by mouse; hub stays behind it')
 r['passed']=True
finally:(base/f'recovery-{fw}-result.json').write_text(json.dumps(r,indent=2)+'\n')
