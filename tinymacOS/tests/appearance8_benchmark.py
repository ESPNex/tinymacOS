"""M8 local appearance benchmark. Usage: python3 tests/appearance8_benchmark.py /external/image.iso /external/result.json
One ordered run is not a statistical or physical-hardware performance claim.
"""
from pathlib import Path
import os,sys,time,json,hashlib,tempfile
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent))
from retail_guest import Retail
from guest import KEYS
from interaction7_checks import geometry
iso=Path(sys.argv[1]).resolve();w=Path(tempfile.mkdtemp(prefix='tmc-appearance8-baseline-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TMC_MEM='768',TMC_SMP='2',TMC_POINTER='tablet',TMC_QA_KEY_HOLD='.07',TMC_QA_KEY_GAP='.10')
class QA(Retail):
 def type(self,text):
  for c in text:self.key(KEYS.get(c,('shift-'+c.lower() if c.isupper() else c)),dt=.10,hold=.07)
def launch(g,a,label):g.key('meta_l-spc');g.type(a);g.key('ret');g.visible(label);time.sleep(.5)
r={'work':str(w),'iso_sha256':hashlib.sha256(iso.read_bytes()).hexdigest(),'source_commit':os.environ.get('TMC_SOURCE_COMMIT','unspecified; identify by ISO hash'),'measurements':[],'physical_hardware':False}
with QA(iso,w,ev,'appearance8-baseline',live=True) as g:
 g.enroll(name='AppearanceQA')
 for app,label in [('Calcolatrice','Cronologia'),('Calendario','Calendario'),('Note','Tutte le note'),('Memo','Tutti i memo'),('Impostazioni','Riduci movimento')]:launch(g,app,label)
 for kind,targets in [('dark',['Scuro','Chiaro','Scuro']),('wall',[1,2,0])]:
  for target in targets:
   if kind=='dark':x,y=g.visible(target,faces=(0,));x+=10;y+=4
   else:
    x,y=g.visible('Colore dettagli',faces=(6,));gx,gy,_=geometry('Colore dettagli',6);x=x-gx+18+83*target+38;y=y-gy+119
   g.move(x,y);time.sleep(.4);before=np.asarray(g.snap())[300:600,20:170].copy();start=time.monotonic();g.req('input-send-event',events=[{'type':'btn','data':{'down':True,'button':'left'}}]);time.sleep(.04);g.req('input-send-event',events=[{'type':'btn','data':{'down':False,'button':'left'}}]);last=before;stable=0;first=None;changes=[]
   until=start+12
   while time.monotonic()<until:
    a=np.asarray(g.snap())[300:600,20:170].copy();t=time.monotonic()-start
    if not np.array_equal(a,last):changes.append(t);first=t if first is None else first;stable=0
    else:stable+=1
    last=a
    if first is not None and stable>=5:break
    time.sleep(.05)
   assert first is not None,(kind,target)
   row={'kind':kind,'target':target,'first_observed_change_seconds':first,'last_observed_change_seconds':changes[-1],'distinct_observed_frames':len(changes),'observation':'real unobstructed wallpaper ROI; stable for five captures after final change; host input-to-frame, not GPU FPS'};r['measurements'].append(row);print(row,flush=True);g.shot(kind+'-'+str(target))
  time.sleep(.3)
Path(sys.argv[2]).resolve().write_text(json.dumps(r,indent=2)+'\n')
