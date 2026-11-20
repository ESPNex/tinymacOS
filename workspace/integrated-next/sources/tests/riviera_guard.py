"""Negative tests modifying disposable test disks only; no guest serial logging needed."""
import sys,os,time,json,hashlib
from pathlib import Path
from retail_guest import Retail
from pixels import locate
W=Path(sys.argv[1]).resolve();W.mkdir(parents=True,exist_ok=True);E=W/'evidence';E.mkdir(exist_ok=True);D=Path(os.environ['TMC_BUILD']);iso=D/'tinymacOS-2.4.4-x86_64.iso';records=[]
def ok(s):records.append(s);(E/'guard.json').write_text(json.dumps(records,indent=2,ensure_ascii=False));print('OK:',s,flush=True)
sentinel=W/'guard.raw';
with sentinel.open('xb') as f:f.truncate(64<<20);f.seek(33<<20);f.write(b'DOCUMENTO DA CONSERVARE')
def h():return hashlib.sha256(sentinel.read_bytes()).hexdigest()
orig=h();os.environ['DISK']=str(sentinel)
with Retail(iso,W,E,'guard',disk='protect') as g:
 g.enroll();g.key('f11');g.visible('Introduzione');
 for k in ('ret','down','ret','spc','ret'):g.key(k,.22)
 g.visible('Il disco contiene dati:',65);g.shot('disco-protetto')
assert h()==orig;ok('Disco apparentemente vuoto con documento al MiB 33: installazione rifiutata e SHA-256 dell’intero disco invariato: '+orig)
# The only storage medium in this VM is the shipping CD.
from boot_recovery import RecoveryKey
class Optical(RecoveryKey,Retail):pass
os.environ.pop('DISK')
with Optical(iso,W,E,'optical-recovery',disk='cd-recovery',live=True) as g:
 g.shot('recupero-da-iso');block=g.req('query-block');usb=g.hmp('info usb');assert str(iso) in str(block) and 'usb-storage' not in str(block) and 'USB MSD' not in usb and 'system.raw' not in str(block)
 g.key('ret');g.visible('Nessun disco disponibile.');g.key('esc');g.visible('Scegli come proseguire.');ok('R sulla ISO avvia il Recupero senza disco SATA o USB storage; nessuna destinazione fittizia')
 for k in ('down','down','down','ret'):g.key(k,.18)
 g.visible('Shell di recupero locale');g.cmd('sw_vers','tinymacOS Recovery 2.4.4');g.cmd('diskutil info /','Origine: ISO di installazione');g.shot('shell-da-iso');ok('Nella shell di Recupero avviata dal CD: sw_vers e percorso/origine del vero BaseSystem.dmg')
