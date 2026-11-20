"""No writes to an unfamiliar SATA disk, even when only its middle contains data."""
import argparse, hashlib, json, os, tempfile, time
from pathlib import Path
from guest import Guest, default_iso
p=argparse.ArgumentParser();p.add_argument('--evidence',required=True,type=Path);a=p.parse_args();E=a.evidence.resolve();E.mkdir(exist_ok=True,parents=True);iso=Path(os.environ.get('ISO',default_iso()));res=[]
def sha(f):return hashlib.sha256(f.read_bytes()).hexdigest()
with tempfile.TemporaryDirectory(prefix='orizzonte-guard-') as tmp:
    w=Path(tmp);d=w/'persistent';d.mkdir();disk=d/'system.raw'
    with disk.open('xb') as f:f.truncate(64<<20);f.seek(29*1024*1024+73);f.write(b'pre-existing data, no partition signature')
    before=sha(disk)
    with Guest(iso,w,E,'nonempty') as g:
        g.enroll();g.spawn('f11','Installa tinymacOS');g.key('ret');g.key('down');g.key('ret');g.key('ret');time.sleep(.20);assert '[install] Controllo del disco' not in g.text() and sha(disk)==before
        res.append('Selection and Return cannot start a scan or write before the separate confirmation.');g.key('spc');g.key('ret');g.wait('annullata senza scrivere',mark=0,timeout=40);g.shot('destinazione-rifiutata');assert '[install] Copia del sistema' not in g.text();g.spawn('f2','Terminale');g.shutdown()
    assert before==sha(disk);res.append('A signatureless drive with a sentinel at 29 MiB is refused during the full scan. All 64 MiB remain byte-identical.')
    with Guest(iso,w,E,'no-target',live=True,disk='no-drive') as g:
        g.enroll();g.spawn('f11','Installa tinymacOS');g.key('ret');g.key('down');g.key('ret');g.key('spc');g.key('ret');time.sleep(.25);g.shot('nessun-disco');assert '[ahci] SATA 0' not in g.text() and '[install] Controllo' not in g.text();g.spawn('f2','Terminale');g.shutdown()
    res.append('No SATA drive: live mode still works and the installer cannot start.')
(E/'disk-guard.json').write_text(json.dumps({'sha256':sha(iso),'unchanged_disk_sha256':before,'checks':res},indent=2)+'\n');print('\n'.join('PASS: '+r for r in res))
