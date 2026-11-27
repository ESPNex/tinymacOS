"""M1 development ISO: real USB input and screenshot-only text/caret checks.
No injected application events, debug syscall or guest-memory writes.
"""
from pathlib import Path
import hashlib,json,os,tempfile,time
import numpy as np
from retail_guest import Retail
iso=Path(os.environ['TMC_STAGE_ISO']);qa=Path(os.environ['TMC_QA_DIR']);qa.mkdir(parents=True,exist_ok=True)
w=Path(tempfile.mkdtemp(prefix='tmc-m1-caret-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet')
r={'passed':False,'development_only':True,'release_acceptance':False,'physical_hardware_tested':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'checks':[]}
def ok(text):r['checks'].append(text);print('PASS',text,flush=True)
def blinking(g,label,crop=(0,50,1280,670)):
    time.sleep(2)
    previous=np.asarray(g.snap().crop(crop));transitions=[]
    for i in range(24):
        time.sleep(.14);current=np.asarray(g.snap().crop(crop))
        mask=np.any(previous!=current,axis=2)
        if mask.any():
            ys,xs=np.nonzero(mask);box=[int(xs.min()),int(ys.min()),int(xs.max()-xs.min()+1),int(ys.max()-ys.min()+1)]
            assert box[2]<=3 and 10<=box[3]<=18,(label,'unexpected non-caret changes',box)
            transitions.append(box)
        previous=current
    assert len(transitions)>=2,(label,'no repeated visible blink')
    r.setdefault('blink_observations',{})[label]=transitions
    g.shot('m1-'+label)
    ok(label+': repeated screenshot changes confined to a single insertion marker')
try:
    with Retail(iso,w,ev,'m1-text-caret',live=True) as g:
        blinking(g,'setup-empty-field',crop=(300,200,980,650))
        g.enroll('CaretQA');g.move(60,700)
        g.key('f2');g.visible('Sessione locale');blinking(g,'Terminal')
        g.cmd('open -a TextEdit');g.visible('Testo semplice');time.sleep(.7)
        g.type('à'*60+'CODAFINE');g.visible('CODAFINE',faces=(5,))
        ok('TextEdit displays tail after 120 UTF-8 accent bytes, formerly truncated by one Draw.text command')
        blinking(g,'TextEdit')
        # Real editor actions, no application event injection.
        g.key('home');g.type('INIZIO');g.visible('INIZIO',faces=(5,))
        g.key('end');g.key('ret');g.type('SECONDA');g.visible('SECONDA',faces=(5,))
        ok('TextEdit Home/End and newline accept real USB keyboard input')
        g.shot('m1-textedit-utf8-tail-and-newline')
        g.key('f2');g.visible('Sessione locale');g.cmd('open -a Note');g.visible('Tutte le note');blinking(g,'Note-empty')
        g.type('NOTAQATEST');g.visible('NOTAQATEST',faces=(5,));ok('Note body accepts text with insertion marker')
        g.key('f2');g.visible('Sessione locale');g.cmd('open -a Memo');g.visible('Tutti i memo');blinking(g,'Memo-empty')
        g.type('MEMOQATEST');g.visible('MEMOQATEST',faces=(5,));ok('Memo body accepts text with insertion marker')
        g.key('f2');g.visible('Sessione locale');g.cmd('off');g.p.wait(timeout=20)
        r['passed']=True
finally:
    (qa/'m1-text-caret-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
    for p in w.glob('*.ppm'):p.unlink()
print('PASS M1 real guest text/caret checks',flush=True)
