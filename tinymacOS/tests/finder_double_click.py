"""Real USB pointer clicks against a booted ISO, with a baseline comparison.
TMC_STAGE_ISO selects the candidate. TMC_EXPECT_OLD=1 records the old defect.
Only disposable live RAM and virtual devices are used.
"""
from pathlib import Path
import os,sys,time,struct,json,tempfile,hashlib
from retail_guest import Retail
import retail_guest
# Short labels must not match an intermediate glass/animation frame.
# Use near-exact shipping-font pixels before sending timed input.
_locate=retail_guest.locate
retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.98: _locate(im,text,faces,threshold)
import pixels
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
base=Path(os.environ['TMC_QA_DIR']);base.mkdir(parents=True,exist_ok=True);w=Path(tempfile.mkdtemp(prefix='finder-double-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();iso=Path(os.environ['TMC_STAGE_ISO']);old=os.environ.get('TMC_EXPECT_OLD')=='1'
os.environ.update(OCR_ROOT=os.environ['TOOLROOT'],TMC_MEM='512',TMC_POINTER='tablet')
r={'passed':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'baseline':old,'checks':[]}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
def press(g,x,y):
 g.move(x,y);g.req('input-send-event',events=[{'type':'btn','data':{'down':True,'button':'left'}}]);time.sleep(.11);g.req('input-send-event',events=[{'type':'btn','data':{'down':False,'button':'left'}}]);time.sleep(.04)
def pair(g,x,y,drift=6):
 # Send the timed USB events without waiting for a render-blocked QMP reply
 # between edges. Drain all replies before using the synchronous driver again.
 def send(events):g.f.write((json.dumps({'execute':'input-send-event','arguments':{'events':events}})+'\n').encode())
 def events(px,down):return [{'type':'abs','data':{'axis':axis,'value':round(v*32767/(n-1))}} for axis,v,n in [('x',px,1280),('y',y,800)]]+[{'type':'btn','data':{'down':down,'button':'left'}}]
 start=time.monotonic();send(events(x,True));time.sleep(.09)
 send([{'type':'btn','data':{'down':False,'button':'left'}}]);time.sleep(.09)
 send(events(x+drift,True));time.sleep(.09)
 send([{'type':'btn','data':{'down':False,'button':'left'}}])
 for _ in range(4):
  while True:
   reply=json.loads(g.f.readline())
   if 'error' in reply:raise RuntimeError(reply)
   if 'return' in reply:break
 r.setdefault('pair_host_seconds',[]).append(time.monotonic()-start)
def folder(g):
 g.cmd('open /Users/User/Documents/ClickPair');time.sleep(1.2);return g.visible('One',box=(0,40,1250,700))
try:
 with Retail(iso,w,ev,'double',live=True) as g:
  g.enroll();g.app('f2','Sessione locale')
  for cmd in ['mkdir /Users/User/Documents/ClickPair','mkdir /Users/User/Documents/ClickPair/One','mkdir /Users/User/Documents/ClickPair/One/Nested','mkdir /Users/User/Documents/ClickPair/Two','echo DOUBLE-CLICK-OK > /Users/User/Documents/ClickPair/Demo.txt']:g.cmd(cmd)
  x,y=folder(g);tx,ty=g.visible('Two',box=(0,40,1250,700));press(g,tx+9,ty+7);time.sleep(.1)
  assert pixels.locate(g.snap(),'Nested',faces=(0,1,2,3),threshold=.98) is None
  ok('One click selects an initially unselected directory; it does not open')
  # No expensive OCR/template matching between the two input occurrences.
  pair(g,x+9,y+7);g.move(1160,640);time.sleep(.10 if old else 1)
  opened=pixels.locate(g.snap(),'Nested',faces=(0,1,2,3),threshold=.98) is not None
  if old:
   assert not opened;ok('Published 10.2.1 reproduces defect: second tap with 6 px drift does not open')
   g.shot('baseline-second-tap-not-open')
  else:
   g.visible('Nested',24);ok('Exactly two USB taps open an initially unselected folder, with 6 px tap drift')
   g.shot('two-taps-folder');g.key('backspace');g.visible('Demo.txt');time.sleep(.8)
   x,y=g.visible('Demo.txt',box=(0,40,1250,700));pair(g,x+8,y+7);g.move(1160,640);g.visible('Testo semplice');g.visible('DOUBLE-CLICK-OK');g.shot('two-taps-file');ok('First tap selects a file; exactly the second opens its real bytes in TextEdit')
  r['passed']=True
finally:(base/('finder-double-baseline.json' if old else 'finder-double-result.json')).write_text(json.dumps(r,indent=2)+'\n')
