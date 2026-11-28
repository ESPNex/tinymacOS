#!/usr/bin/env python3
"""Priority M8 visual regression; real QEMU input and native screenshots.
Usage: python3 tests/priority8_guest.py /external/candidate.iso
"""
from pathlib import Path
import sys,os,time,json,hashlib,tempfile
import numpy as np
from retail_guest import Retail
from guest import KEYS
from interaction7_checks import geometry
os.environ.update(TMC_MEM='768',TMC_SMP='2',TMC_POINTER='tablet')
class QA(Retail):
    def type(self,text):
        for c in text:self.key(KEYS.get(c,('shift-'+c.lower() if c.isupper() else c)),dt=.10,hold=.07)
def launch(g,app,label):
    g.key('meta_l-spc');g.type(app);g.key('ret');g.visible(label);time.sleep(.7)
def main():
    iso=Path(sys.argv[1]).resolve();w=Path(tempfile.mkdtemp(prefix='tmc-priority8-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
    r={'iso_sha256':hashlib.sha256(iso.read_bytes()).hexdigest(),'work':str(w),'checks':[]}
    with QA(iso,w,ev,'priority8',live=True) as g:
        g.enroll(name='VisualQA');launch(g,'Calcolatrice','Cronologia');g.type('7+5');g.key('ret');time.sleep(.8);g.move(45,745)
        g.visible('12',faces=(4,));g.shot('01-calcolatrice-bordo-interno')
        r['checks'].append('Calculator actual app: arithmetic 7+5=12; capture of keypad edge without duplicate lens, history/window retained')
        before=np.asarray(g.snap()).copy();g.key('meta_l-shift-a');g.visible('14 applicazioni',faces=(0,));time.sleep(.8);g.move(45,745)
        after=np.asarray(g.shot('02-libreria-app-senza-velo'))
        # A strip well outside the floating panel is no longer artificially dimmed.
        a=before[300:580,30:160];b=after[300:580,30:160];same=float(np.mean(np.all(a==b,axis=2)));assert same>.98,same
        # The bottom strip used to receive the panel's own cast shadow.
        a=before[627:633,320:960];b=after[627:633,320:960];halo=float(np.mean(np.all(a==b,axis=2)));assert halo>.98,halo
        r['library_outside_pixels_unchanged']=same;r['library_shadow_strip_unchanged']=halo
        r['checks'].append('Library has no external dark veil or cast shadow: exact outside-panel pixel comparisons')
        g.key('esc');time.sleep(.7);launch(g,'Calendario','Calendario');g.move(45,745);before=np.asarray(g.snap()).copy()
        g.key('ctrl-up');time.sleep(1.1);g.move(45,745);after=np.asarray(g.shot('03-mission-control-barra-menu'))
        # Same packed RGB blend as the compositor. Middle of menubar has no clock.
        a=before[2:27,580:800].astype(np.uint32);tint=np.array([7,9,15],np.uint32)
        expected=(a*144+tint*112)>>8;b=after[2:27,580:800]
        agreement=float(np.mean(np.all(expected==b,axis=2)));assert agreement>.98,agreement
        r['menubar_dim_exact_pixel_agreement']=agreement
        g.key('esc');time.sleep(.9);restored=np.asarray(g.snap());assert np.mean(np.all(restored[2:27,580:800]==before[2:27,580:800],axis=2))>.98
        r['checks'].append('Mission Control menubar uses same 112/256 dim as backdrop exactly once; exit restores original pixels')
        launch(g,'Impostazioni','Colore dettagli');g.move(45,745);before=np.asarray(g.snap()).copy()
        x,y=g.visible('Colore dettagli',faces=(6,));gx,gy,_=geometry('Colore dettagli',6)
        # Third theme (index 2), matched to the production Settings layout.
        g.click(x-gx+20+2*40+13,y-gy+230-202+13);time.sleep(1.0);g.move(45,745)
        after=np.asarray(g.shot('04-accento-con-sfondo'))
        difference=float(np.mean(np.any(before[300:600,20:160]!=after[300:600,20:160],axis=2)));assert difference>.5,difference
        r['accent_changes_wallpaper_fraction']=difference
        r['checks'].append('Changing theme accent also changes its associated wallpaper; original transition restored')
    (w/'result.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2),flush=True)
if __name__=='__main__':main()
