"""Functional cold-boot and recovery acceptance; all destructive cases use fresh copies of our QA disk."""
import sys,os,time,json,shutil,hashlib,struct
from pathlib import Path
from retail_guest import Retail
from boot_recovery import RecoveryKey
from pixels import locate
from riviera_media import gpt,audit_bytes,H
SRC=Path(sys.argv[1]).resolve();W=Path(sys.argv[2]).resolve();W.mkdir(parents=True,exist_ok=False);E=W/'evidence';E.mkdir();D=Path(os.environ['TMC_BUILD']);disk=W/'installed.raw';expected=gpt(SRC);assert audit_bytes(expected['data'])['/Users/User/Desktop/riva.txt']==b'Riviera\n';shutil.copyfile(SRC,disk);results=[]
def ok(s):results.append(s);(E/'disk.json').write_text(json.dumps(results,indent=2,ensure_ascii=False));print('OK:',s,flush=True)
class Locked(Retail):initial='Elena'
class Rec(Retail):initial='Scegli come proseguire.'
def keyseq(g,*keys):
 for k in keys:g.key(k,.22)
def off(g):g.key('f2');g.visible('su console');g.type('off');g.key('ret');g.p.wait(timeout=15);assert g.p.returncode==0
os.environ['DISK']=str(disk)
with Locked('/not-attached',W,E,'disk',disk='fresh-vars',boot_disk=True) as g:
 block=g.req('query-block');assert all('.iso' not in str(x) for x in block);g.shot('13-login-disco');g.type('local-test-285');g.key('ret',0);g.visible('Verifica della password',5,faces=(0,));g.shot('14-accesso');g.visible('Preferiti',40);time.sleep(.7);assert not locate(g.frame(),'Installa tinymacOS');ok('NVRAM nuova, nessuna ISO: accesso con l’account creato sulla live; scrivania senza installer')
 g.key('f11',.7);assert not locate(g.frame(),'Introduzione');g.key('ctrl-shift-a');g.visible('13 applicazioni');g.shot('15-libreria-disco');assert not locate(g.frame(),'Installa tinymacOS');g.key('esc',.45);g.key('ctrl-spc');g.type('Installa');g.visible('Nessun risultato.');g.key('esc',.4);ok('F11 non avvia un installer; 13 app nella libreria del disco e nessun risultato cercando Installa')
 g.key('f2');g.visible('su console');g.cmd('cat Desktop/riva.txt');g.cmd('cat Documents/prova/conta');g.cmd('ls /Applications');g.cmd('open -a "Install tinymacOS"','Applicazione non trovata.');g.cmd('cd "/Volumes/Macintosh HD/Library"');g.cmd('ls','Preferences');g.cmd('cd');g.cmd('diskutil verifyVolume disk0','Il volume risulta valido.');g.cmd('open -a "Disk Utility"','Dispositivi e volumi');g.shot('16-utility-disco');g.key('s');g.visible('Esegui SOS');g.key('ret');g.visible('SOS completato.',45);g.key('b');g.visible('Crea una copia locale');g.key('ret');g.visible('Copia locale salvata e verificata.',40);ok('Macintosh HD montato in /Volumes, diskutil e Utility Disco hanno controllato il disco e salvato una copia verificata')
 g.key('ctrl-w',.6);g.key('f2',.3);g.cmd('echo Successivo > Desktop/riva.txt');g.cmd('open -a "Disk Utility"','Dispositivi e volumi');g.key('r');g.visible('Apri Recupero');g.key('ret');g.visible('Scegli come proseguire.',52);g.shot('17-recupero-disco');ok('Riavvio da Utility Disco alla partizione di Recupero sullo stesso disco, senza supporti aggiuntivi')
 keyseq(g,'ret','ret');g.visible('Conferma l’operazione');g.key('ret');g.visible('Copia ripristinata.',30);g.shot('18-ripristino-dati');g.key('ret');g.visible('Scegli come proseguire.');keyseq(g,'up','ret','ret');g.visible('Elena',50);g.unlock();g.key('f2');g.visible('su console');g.cmd('cat Desktop/riva.txt');g.cmd('cat Desktop/riva.txt > Documents/dalback.txt');off(g)
a=gpt(disk);data=audit_bytes(a['data']);assert data['/Users/User/Desktop/riva.txt']==data['/Users/User/Documents/dalback.txt']==b'Riviera\n';assert a['seal'].startswith(b'TMCSNAP2') and a['seal'][32:64]==hashlib.sha256(a['snapshot']).digest();ok('Ripristino reale della copia, scelta del disco di avvio, nuovo login e lettura dei byte precedenti alla modifica')
# Disjoint sacrificial clones: never patch a master ISO, an existing user disk, or a running VM.
p=W/'repair.raw';shutil.copyfile(disk,p);os.environ['DISK']=str(p)
with p.open('r+b') as f:f.seek(512+16);v=f.read(1);f.seek(-1,1);f.write(bytes([v[0]^32]))
class KeyRec(RecoveryKey,Retail):pass
with KeyRec(D/'tinymacOS-2.4.3-x86_64.iso',W,E,'repair',disk='repair-vars') as g:
 keyseq(g,'down','down','ret','ret');g.visible('SOS: controlla e ripara');g.key('ret');g.visible('SOS completato.',45);g.shot('19-sos-gpt')
repaired=gpt(p);assert repaired['parts']==a['parts'] and repaired['id']==a['id'] and repaired['data']==a['data'];ok('BaseSystem interno alla ISO: SOS ha ricostruito la GPT primaria danneggiata dalla secondaria; CRC corretti e dati identici')
# Corrupt a recoverable area of the installed kernel payload, leaving Recovery and its clean source unaffected.
where=repaired['esp'].index((D/'KernelCache').read_bytes())+128
with p.open('r+b') as f:f.seek(2048*512+where);v=f.read(1);f.seek(-1,1);f.write(bytes([v[0]^8]))
os.environ.pop('RECOVERY_IMG',None)
with Rec('/not-attached',W,E,'reinstall',disk='restore-vars',boot_disk=True) as g:
 ok('KernelCache corrotto nella copia di prova: il loader rifiuta il digest e apre automaticamente Recupero dal disco')
 keyseq(g,'down','ret','ret');g.visible('Conferma l’operazione');g.key('ret');g.visible('tinymacOS è stato reinstallato.',60);g.shot('20-reinstallazione');g.key('ret');g.visible('Scegli come proseguire.');keyseq(g,'up','up','ret','ret');g.visible('Elena',60);g.unlock();g.key('f2');g.visible('su console');g.cmd('cat Desktop/riva.txt');off(g)
reinstalled=gpt(p);assert reinstalled['esp']==a['esp']==a['src'] and reinstalled['data']==a['data'];ok('Reinstallazione offline: ESP intera byte-per-byte uguale all’originale, volume dati invariato e desktop riavviato')
with KeyRec('/not-attached',W,E,'erase',disk='erase-vars',boot_disk=True) as g:
 keyseq(g,'down','down','ret','ret','down','down','ret');g.visible('Scrivi INIZIALIZZA');g.type('NO');g.key('ret');g.visible('Scrivi INIZIALIZZA');g.key('esc');g.visible('SOS: controlla e ripara');keyseq(g,'ret');g.visible('Scrivi INIZIALIZZA');g.type('INIZIALIZZA');g.key('ret');g.visible('Volume dati inizializzato.',45);g.shot('21-inizializzato')
b=gpt(p);assert b['esp']==a['esp'] and b['seal']==bytes(512) and not any(b['snapshot']);assert b['data'][13*512:15*512]==bytes(1024) and b['data'][15*512:15*512+8]==b'TMFACT20';os.environ.pop('RECOVERY_IMG',None)
class Fresh(Retail):initial='Crea un account'
with Fresh('/not-attached',W,E,'new-account',disk='new-vars',boot_disk=True) as g:
 g.enroll('Marta','second-test-81');g.key('f2');g.visible('su console');g.cmd('whoami > Documents/account.txt');off(g)
z=audit_bytes(gpt(p)['data']);assert z['/Users/User/Documents/account.txt']==b'Marta\n' and '/Users/User/Desktop/riva.txt' not in z;ok('Inizializza con parola di conferma: vecchio account, documenti e copia locale cancellati; il sistema installato consente di creare un nuovo account')
