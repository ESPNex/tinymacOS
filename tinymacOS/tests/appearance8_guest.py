#!/usr/bin/env python3
"""M8 real guest: proportional Spotlight editing and vertical shortcuts.
Usage: python3 tests/appearance8_guest.py /external/candidate.iso
Artifacts stay outside the source/workspace. No device/volume mutation.
"""
from pathlib import Path
import os,sys,time,json,tempfile,hashlib
from retail_guest import Retail
from guest import KEYS
from interaction7_checks import geometry
os.environ.update(TMC_MEM='768',TMC_SMP='2',TMC_POINTER='tablet',TMC_QA_KEY_HOLD='.07',TMC_QA_KEY_GAP='.10')
class QA(Retail):
    def type(self,text):
        for c in text:self.key(KEYS.get(c,('shift-'+c.lower() if c.isupper() else c)),dt=.10,hold=.07)
def main():
    iso=Path(sys.argv[1]).resolve();work=Path(tempfile.mkdtemp(prefix='tmc-appearance8-ui-',dir='/var/tmp'));ev=work/'evidence';ev.mkdir()
    report={'iso_sha256':hashlib.sha256(iso.read_bytes()).hexdigest(),'work':str(work),'checks':[]}
    with QA(iso,work,ev,'appearance8-ui',live=True) as g:
        g.enroll(name='SpotlightQA');g.key('meta_l-spc');g.type('iiiWWW');time.sleep(.5)
        gx,gy,_=geometry('iiiWWW',1);x,y=g.visible('iiiWWW',faces=(1,));origin=x-gx
        g.click(origin+geometry('iii',1)[2],y+3);g.type('a');g.move(45,745)
        g.visible('iiiaWWW',faces=(1,));g.shot('spotlight-proportional-click')
        report['checks'].append('Spotlight actual proportional body glyphs; click at font-metric boundary inserts at iii|WWW')
        g.key('meta_l-a');g.type('Wi'*22);g.key('end');g.key('home');g.type('a');g.key('end');g.type('b');g.move(45,745);g.shot('spotlight-long-viewport')
        # Scroll back from a real overflowing query; caret stays on character boundaries.
        for _ in range(3):g.key('left')
        g.key('backspace');g.type('W');g.key('esc')
        report['checks'].append('Long proportional Spotlight query: Home/End, viewport, arrows/backspace/insertion exercised')
        x,y=g.visible('Aiuto',faces=(0,1));g.click(x+10,y+5);x,y=g.visible('Scorciatoie',faces=(1,));g.click(x+20,y+5)
        g.visible('Command · Windows',faces=(0,));g.visible('Option · Alt sinistro',faces=(0,))
        labels=['Spotlight','Mission Control','Applicazioni','Finestra precedente','Chiudi finestra','Riduci a icona','Blocca schermo','Nuova cartella']
        positions=[g.visible(s,faces=(1,)) for s in labels]
        assert all(positions[i+1][1]>positions[i][1] for i in range(len(positions)-1)),positions
        assert max(p[0] for p in positions)-min(p[0] for p in positions)<=4,positions
        g.move(45,745);g.shot('shortcuts-vertical-symbols');report['shortcut_positions']=positions
        # Clicking an action label must NOT activate the old broad confirmation hit region.
        x,y=positions[-1];g.click(x+20,y+4);g.visible('Scorciatoie',faces=(3,));g.visible('Fine',faces=(0,))
        x,y=g.visible('Fine',faces=(0,));g.click(x+10,y+5);time.sleep(.7)
        from retail_guest import locate
        assert not locate(g.snap(),'Command · Windows',(0,))
        report['checks'].append('Vertical shortcuts, modifier equivalents, inert content clicks and exact Fine button hit area')
    (work/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2),flush=True)
if __name__=='__main__':main()
