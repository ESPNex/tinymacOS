#!/usr/bin/env python3
"""Independent fresh-disk, no-RNG, host-code and optional real 1.2.2 upgrade tests.
All input through QMP/USB (legacy guest: PS/2), all account checks offline, no guest memory inspection.
"""
import argparse, hashlib, json, os, re, secrets, tempfile, time
from pathlib import Path
from guest import Guest, default_iso, audit_bytes
from orizzonte import install,disk_bytes

ap=argparse.ArgumentParser();ap.add_argument('--evidence',type=Path,required=True);ap.add_argument('--previous',type=Path);ap.add_argument('--previous-launcher',type=Path);args=ap.parse_args()
iso=Path(os.environ.get('ISO',default_iso())).resolve();E=args.evidence.resolve();E.mkdir(exist_ok=True,parents=True)
prev=args.previous.resolve() if args.previous else None
pw='compat-test-238';name='Lucia';limited='max,rdrand=off,rdseed=off';notsc=limited+',tsc=off';T='[user:Terminale] '
res=[];sources={}
def check(ok,label):
    assert ok,label
    print('PASS: '+label,flush=True);res.append(label)
def cfg(cpu,rng='none'):os.environ.update(TMC_CPU=cpu,TMC_RNG=rng)
def watch(g):
    g.wait('public client cannot supply setup salt',mark=0)
    sources[g.tag]=re.findall(r'^\[salt\] (.*)$',g.text(),re.M)
def record(w,sub,old=False):
    raw=(w/sub/'user.img').read_bytes() if old else disk_bytes(w/sub/'system.raw');a=raw[13*512:14*512];salt=a[48:64]
    assert a==raw[14*512:15*512] and a[:8]==b'TMCUSER2' and hashlib.sha256(a[:480]).digest()==a[480:]
    assert int.from_bytes(a[12:16],'little')==120000 and salt!=bytes(16)
    assert a[64:96]==hashlib.pbkdf2_hmac('sha256',pw.encode(),salt,120000) and pw.encode() not in raw
    return salt,a,raw
with tempfile.TemporaryDirectory(prefix='aurora-rng-') as t:
    w=Path(t);salts=[]
    for j in range(2):
        cfg(limited);tag='auto'+str(j)
        with Guest(iso,w,E,tag,disk=tag) as g:
            watch(g);assert sources[tag]==['jitter health checks passed'] and 'host code ready' not in g.text()
            g.enroll(name,pw);install(g)
        salts.append(record(w,tag)[0]);
    check(salts[0]!=salts[1],'Two fresh installations with identical RTC, name/password and no EFI/hardware/virtio RNG: successful enrollment with different salts and correct independent PBKDF2 verifiers')
    cfg(notsc)
    with Guest(iso,w,E,'host-code',disk='manual') as g:
        watch(g);assert sources[g.tag]==['host setup code required'];g.shot('configurazione-solo-se-necessaria')
        for bad in ('1','0'*32):g.type(bad);g.key('ret');g.key('ctrl-u')
        assert 'host setup code accepted' not in g.text() and '[session] authenticated' not in g.text()
        code=secrets.token_hex(16);g.type(code);g.key('ret');g.wait('host setup code accepted',mark=0)
        assert '[window] open' not in g.text() and 'verifier enrolled' not in g.text();g.enroll(name,pw);install(g);assert code not in g.text()
    check(record(w,'manual')[0]==bytes.fromhex(code),'Without TSC or RNG the release prompts before password entry; short/degenerate codes rejected, a fresh host-generated code alone does not unlock anything; real password enrollment succeeds')
    cfg(limited,'host')
    with Guest(iso,w,E,'virtual-device',live=True) as g:
        watch(g);assert 'virtio-rng-pci' in g.hmp('info qtree') and sources[g.tag] in (['EFI RNG'],['jitter health checks passed'])
        g.enroll(name,pw);g.spawn('f2','Terminale');g.shutdown()
    check(True,'Host-backed virtio-rng device attached by the Linux launcher and a live, CPU-RNG-masked boot reaches Finder (actual chosen source is recorded)')
    if prev:
        assert args.previous_launcher, 'Supply the original release launcher as --previous-launcher'
        cfg('max');sub='v122'
        with Guest(prev,w,E,'previous-account',disk=sub,launcher=args.previous_launcher.resolve()) as g:
            assert 'tinymacOS/1.2.2:' in g.text();g.enroll(name,pw);g.spawn('f2','Terminale');g.cmd('write Documents/conserva.txt Prima del cambio ISO',T+'OK');g.shutdown()
        _,oldrec,oldraw=record(w,sub,old=True);(w/sub/'vars.fd').unlink();cfg(notsc)
        with Guest(iso,w,E,'upgrade',disk=sub) as g:
            watch(g);assert not sources[g.tag] and '[session] account locked' in g.text() and '[window] open' not in g.text()
            g.type('not-the-password');m=g.mark();g.key('ret');g.wait('authentication refused',mark=m);assert '[session] authenticated' not in g.text()[m:]
            time.sleep(1.4);g.unlock(pw);g.spawn('f2','Terminale');g.cmd('cat Documents/conserva.txt',T+'Prima del cambio ISO');install(g)
        _,newrec,newraw=record(w,sub)
        assert (w/sub/'user.img').read_bytes()==oldraw and oldrec==newrec
        with Guest(None,w,E,'imported-disk-alone',disk=sub,boot_disk=True) as g:
            g.unlock(pw);g.spawn('f2','Terminale');g.cmd('cat Documents/conserva.txt',T+'Prima del cambio ISO');g.shutdown()
        check(True,'Account created with the real 1.2.2 CD: a 1.3 live read-only import rejects the wrong password, preserves the verifier, installs to a second SATA disk, and recovers its document without CD or RNG. The entire original 8 MiB image is unchanged.')
    for p in E.glob('*-serial.txt'):
        text=p.read_text();assert all(a not in text for a in (pw,code,'[exception]','PANIC:'))
    check(True,'Focused test transcripts contain no test password, host-generated code, exception or panic')
metrics={'iso_sha256':hashlib.sha256(iso.read_bytes()).hexdigest(),'previous_sha256':None if not prev else hashlib.sha256(prev.read_bytes()).hexdigest(),'sources_per_boot':sources,'checks':res}
(E/'salt-checks.json').write_text(json.dumps(metrics,indent=2)+'\n');print(len(res),'focused checks passed')
