#!/usr/bin/env python3
"""Real three-window Mission Control: old native geometry must not leak into glass.
No debug hooks or action injection; move Finder using the actual title bar.
"""
from pathlib import Path
import os,sys,time,json,hashlib,tempfile
import numpy as np
from retail_guest import Retail
from guest import KEYS
from interaction7_checks import geometry
os.environ.update(TMC_MEM='768',TMC_SMP='2',TMC_POINTER='tablet')
class QA(Retail):
    def type(self,text):
        for c in text:self.key(KEYS.get(c,('shift-'+c.lower() if c.isupper() else c)),dt=.10,hold=.07)
def launch(g,app,label):
    g.key('meta_l-spc');g.type(app);g.key('ret');g.visible(label);time.sleep(.8)
def main():
    iso=Path(sys.argv[1]).resolve();w=Path(tempfile.mkdtemp(prefix='tmc-mc8-optics-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
    r={'work':str(w),'iso_sha256':hashlib.sha256(iso.read_bytes()).hexdigest(),'checks':[]}
    with QA(iso,w,ev,'mc8-optics',live=True) as g:
        g.enroll(name='VisualQA');launch(g,'Finder','Preferiti');launch(g,'Calcolatrice','Cronologia');g.type('7+5');g.key('ret')
        launch(g,'Calendario','Calendario');g.move(45,745);time.sleep(1)
        g.shot('01-native-overlapping-windows')
        g.key('ctrl-up');time.sleep(2.5);g.move(45,745)
        a=np.asarray(g.shot('02-mission-control-live-optics')).copy()
        # Warm another composition pass: no rectangular Dock padding may replay.
        g.move(40,740);time.sleep(.4);g.move(45,745);time.sleep(.5)
        a2=np.asarray(g.shot('03-mission-control-steady-optics'))
        same=float(np.mean(np.all(a[650:785,45:1000]==a2[650:785,45:1000],axis=2)))
        assert same>.995,same;r['calendar_dock_region_steady_agreement']=same
        g.key('esc');time.sleep(1.5);launch(g,'Finder','Preferiti')
        x,y=g.visible('Preferiti',faces=(0,));gx,gy,_=geometry('Preferiti',0);cx=x-gx-16;cy=y-gy-14
        before=np.asarray(g.snap()).copy();g.drag(cx+210,cy-20,320,-125);time.sleep(1.0);g.move(45,745)
        moved=np.asarray(g.shot('04-finder-native-position-changed'))
        changed=float(np.mean(np.any(before[80:680,80:1150]!=moved[80:680,80:1150],axis=2)));assert changed>.05,changed
        r['native_geometry_changed_fraction']=changed
        # Restore exactly the same z-order/focus as in the first overview.
        launch(g,'Calcolatrice','Cronologia');launch(g,'Calendario','Calendario');g.move(45,745);time.sleep(.8)
        g.key('ctrl-up');time.sleep(2.5);g.move(45,745)
        b=np.asarray(g.shot('05-overview-after-native-finder-move'))
        # MC slots depend on window size/order, not the old native origin.
        # Compare both entire calculator/calendar tiles, including transparent
        # areas; the changed original Finder position must have zero influence.
        for name,box in [('calculator',(720,76,1245,420)),('calendar',(35,430,610,780))]:
            l,t,rr,bb=box;agreement=float(np.mean(np.all(a[t:bb,l:rr]==b[t:bb,l:rr],axis=2)))
            assert agreement>.995,(name,agreement)
            r[name+'_old_position_invariance']=agreement
        g.key('esc');time.sleep(1.5);g.visible('Calendario');g.shot('06-exit-restores-native-windows')
        r['checks'].append('Three native windows, live MC glass at destination, warm Dock region stable; original Finder moved by title drag; same MC slots retain calculator/calendar pixels; exit restores native windows')
    (w/'result.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2),flush=True)
if __name__=='__main__':main()
