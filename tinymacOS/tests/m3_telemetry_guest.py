"""Black-box M3: actual sysstat threads/sleep/alloc, raw S_INFO/S_PS, native IO.
Guest writes JSONL and GUI installs it; host extracts read-only after power-off.
No injected counters, QA backdoor, preformatted data or physical disk.
"""
from pathlib import Path
import hashlib,json,os,struct,subprocess,tempfile,time
from retail_guest import Retail
from guest import KEYS
iso=Path(os.environ['TMC_STAGE_ISO']);qa=Path(os.environ['TMC_QA_DIR']);qa.mkdir(parents=True,exist_ok=True)
cpus=int(os.environ.get('TMC_M3_CPUS','4'));assert cpus in (1,4,8)
timer=os.environ.get('TMC_M3_TIMER','apic');assert timer in ('apic','pit') and (timer=='apic' or cpus==1)
w=Path(tempfile.mkdtemp(prefix='tmc-m3-guest-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();disk=w/'target/system.raw'
os.environ.update(TMC_MEM='512',TMC_SMP=str(cpus),TMC_POINTER='tablet',TMC_STORAGE='nvme')
launcher=None
if timer=='pit':
    launcher=w/'pit-launch.sh';launcher.write_text('#!/bin/bash\nexec '+str(Path(__file__).resolve().parents[1]/'tools/run-qemu.sh')+' "$@" -cpu max,-apic\n');launcher.chmod(0o755)
r={'passed':False,'release_ready':False,'physical_hardware_tested':False,'requested_cpus':cpus,'timer':timer,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'checks':[]}
class QA(Retail):
    def type(self,text):
        for c in text:self.key(KEYS.get(c,('shift-'+c.lower() if c.isupper() else c)),dt=.055,hold=.025)
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
try:
    with QA(iso,w,ev,'m3-live',disk='target',live=False,launcher=launcher) as g:
        g.enroll(name='TelemetryQA');g.key('f8');g.visible('Monitoraggio in tempo reale');g.visible('Memoria gestita in uso');g.visible('CPU '+str(cpus-1));g.move(50,700);g.shot('m3-monitor-'+str(cpus)+'cores')
        g.key('spc');g.visible('Riprendi');x,y=g.visible('CPU attiva');a=g.snap().crop((x,y,x+290,y+140));time.sleep(1.4);g.key('s');time.sleep(.4);b=g.snap().crop((x,y,x+290,y+140));assert a.tobytes()==b.tobytes(),'Pause/sort changed CPU card'
        g.key('spc');g.visible('Pausa');g.key('ctrl-w');g.key('f2');g.move(50,700);g.visible('Sessione locale');g.cmd('sysstat test > Documents/m3-result.jsonl');time.sleep(2);g.visible('Sessione locale',240);g.cmd('wc -l Documents/m3-result.jsonl');g.visible('13');g.shot('m3-native-result-written')
        g.key('f11');g.visible('Installa tinymacOS Tramontana');g.key('ret');g.visible('Seleziona la destinazione');g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.key('spc');g.key('ret');g.visible('Il disco è pronto',600);g.key('ret');g.p.wait(timeout=20)
    ok('Real Monitor '+str(cpus)+'-CPU bars and allocator labels; pause/sort freezes CPU card and resume works; guest sysstat completed actual threads/sleep/alloc/VM test and saved 13 native records')
    with disk.open('rb') as f:f.seek(1024+128+32);a,b=struct.unpack('<QQ',f.read(16))
    root=Path(__file__).resolve().parents[1];exe=w/'read-results'
    subprocess.run(['gcc','-O2','-fno-builtin','-I'+str(root/'include'),str(root/'tests/m3_read_result.c'),str(root/'storage/tmcbfs2.c'),str(root/'kernel/string.c'),'-o',str(exe)],check=True)
    raw=subprocess.check_output([str(exe),str(disk),str(a),str(b-a+1)]);(qa/('m3-'+str(cpus)+'-'+timer+'-raw.jsonl')).write_bytes(raw)
    rows=[json.loads(x) for x in raw.decode().splitlines()];d={x['phase']:x for x in rows};assert len(rows)==len(d)==13
    for x in rows:
        assert x['mask']==(1<<cpus)-1 and len(x['cores'])==8 and len(x['self'])==5
        total,free,ram,reserved,user=x['mem'];assert 0<=user<=total-free<=total<=ram and reserved==ram-total
        assert x['self'][3]<=user and x['self'][4]==cpus+1
        if 'busy' in x:assert x['busy']==sum(y[1] for y in x['cores'])
        for i,(t,b) in enumerate(x['cores']):assert 0<=b<=t and (t>0 if i<cpus else t==b==0)
    for x,y in zip(rows,rows[1:]):
        assert x['wall']<=y['wall']
        assert x['self'][0]==y['self'][0] and x['self'][1]<=y['self'][1] and x['self'][2]<=y['self'][2]
        for (t,b),(T,B) in zip(x['cores'],y['cores']):assert t<=T and b<=B
    ratios={}
    for phase in ['idle','one','multi','sleep']:
        a,b=d[phase+'0'],d[phase+'1'];dt=sum(y[0]-x[0] for x,y in zip(a['cores'],b['cores']));db=sum(y[1]-x[1] for x,y in zip(a['cores'],b['cores']));dp=b['self'][2]-a['self'][2];stamp=b['self'][1]-a['self'][1]
        assert 0<=db<=dt and dt>100 and abs(dt-stamp)<=max(12,dt*.04),(phase,'incoherent total/process timer window',dt,stamp)
        ratios[phase]={'total_pct':100*db/dt,'process_pct':100*dp/stamp,'samples':dt,'wall_ticks':b['wall']-a['wall'],'per_core_pct':[100*(y[1]-x[1])/(y[0]-x[0]) for x,y in zip(a['cores'][:cpus],b['cores'][:cpus])]}
    assert ratios['idle']['process_pct']<20 and ratios['sleep']['process_pct']<20,ratios
    assert 45/cpus<=ratios['one']['process_pct']<=min(100,125/cpus),ratios
    assert ratios['multi']['process_pct']>=55 and ratios['multi']['total_pct']>=65,ratios
    if cpus>1:assert ratios['multi']['process_pct']>ratios['one']['process_pct']+20,ratios
    assert ratios['sleep']['total_pct']<ratios['multi']['total_pct']-20,ratios
    r['intervals']=ratios
    ok('Raw coherent real scheduler windows: idle/sleep low, one worker normalized by '+str(cpus)+' cores, all-core load rises; monotonic per-core and shared-process samples without underflow')
    assert d['alloc']['mem'][4]-d['mem0']['mem'][4]==8*1024*1024
    assert d['alloc']['self'][3]-d['mem0']['self'][3]==8*1024*1024
    assert d['mem0']['mem'][1]-d['alloc']['mem'][1]>=8*1024*1024
    assert d['free']['mem'][4]==d['mem0']['mem'][4] and d['free']['self'][3]==d['mem0']['self'][3]
    assert d['commit']['mem'][4]-d['free']['mem'][4]==4*1024*1024
    assert d['commit']['self'][3]-d['free']['self'][3]==4*1024*1024
    assert d['release']['mem'][4]==d['free']['mem'][4] and d['release']['self'][3]==d['free']['self'][3]
    # Empty page tables remain until process teardown: at most a few pages, not a 12MiB leak.
    assert 0<=d['mem0']['mem'][1]-d['release']['mem'][1]<=16*4096
    r['memory']={k:d[k]['mem'] for k in ['mem0','alloc','free','commit','release']}
    ok('Actual S_ALLOC 8MiB and VM commit 4MiB grow pool/user/per-process resident counters exactly; free/release return all user backing, only bounded empty page-table overhead remains')
    class Installed(QA):initial='TelemetryQA'
    with Installed('/not-attached',w,ev,'m3-cold',disk='target',boot_disk=True,launcher=launcher) as g:
        g.unlock();g.key('f2');g.move(50,700);g.visible('Sessione locale');g.cmd('wc -l Documents/m3-result.jsonl');g.visible('13');g.key('f8');g.visible('CPU '+str(cpus-1));g.visible('Memoria gestita in uso');g.shot('m3-cold-monitor-'+str(cpus)+'cores')
    ok('Cold native boot without ISO retains the guest-written report and functional Monitor telemetry')
    r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:
    (qa/('m3-'+str(cpus)+'-'+timer+'-result.json')).write_text(json.dumps(r,indent=2)+'\n')
    for p in w.glob('*.ppm'):p.unlink()
