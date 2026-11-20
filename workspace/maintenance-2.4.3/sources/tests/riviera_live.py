"""Create a new test account and install the actual released ISO through the retail UI."""
import os,sys,time,json,hashlib
from pathlib import Path
from retail_guest import Retail,locate
from riviera_media import medium,gpt,audit_bytes,DATA,H
W=Path(sys.argv[1]).resolve();W.mkdir(parents=True,exist_ok=True);E=W/'evidence';E.mkdir();D=Path(os.environ['TMC_BUILD']);iso=D/'tinymacOS-2.4.3-x86_64.iso';results=[]
def ok(s):results.append(s);(E/'live.json').write_text(json.dumps(results,indent=2,ensure_ascii=False));print('OK:',s,flush=True)
m=medium(iso,W/'catalog',ok)
class Live(Retail):
 initial='Crea un account'
 def wait(self,*a,**k):
  # Real GOP progress bar, not a COM1 instrumentation milestone.
  vals=set();end=time.monotonic()+48
  while time.monotonic()<end:
   im=self.snap()
   if im.size==(1280,800) and im.getpixel((640,378))==(255,255,255) and im.getpixel((40,40))==(0,0,0):
    z=sum(im.getpixel((x,492))[0]>180 for x in range(544,736));vals.add(z)
    if 15<z<148 and not (E/'01-avvio.png').exists():im.save(E/'01-avvio.png')
   elif len(vals)>2 and im.size==(1280,800):break
   time.sleep(.026)
  assert len(vals)>=3,vals;self.visible(self.initial,46);ok(f'UEFI: mela e barra sotto il logo, {len(vals)} livelli osservati prima del setup')
with Live(iso,W,E,'live',disk='sata') as g:
 g.shot('02-account');g.enroll();g.shot('03-desktop');g.key('ctrl-shift-k');g.visible('KernelCache');g.shot('04-kernel');g.key('ctrl-shift-a');g.visible('14 applicazioni');g.shot('05-libreria-live');g.key('esc');g.key('f2');g.visible('su console');
 def cmd(s):g.cmd(s);time.sleep(.05)
 cli=['mkdir Documents/prova','cd Documents/prova','echo "Ciao Riviera" > saluto.txt','cp saluto.txt copia.txt','mv copia.txt sposta.txt','touch vuoto','rm vuoto','mkdir cartella','rmdir cartella','cat sposta.txt | grep Riviera | wc -w > conta','echo Alba >> saluto.txt','sort < saluto.txt | head -n 1 > primo','tail -n 1 saluto.txt > ultimo','basename /Library/Preferences > base','dirname /Library/Preferences > dir','which diskutil > where','whoami > chi','pwd > luogo','uname -m > arch','sw_vers -productVersion > versione','diskutil list > dischi','shasum sposta.txt > sha1','shasum -a 256 /System/Library/KernelCollections/KernelCache > hash','ps > processi','date > data','ls / > root','echo $USER > nome','cd ~','echo Riviera > Desktop/riva.txt','clear','sw_vers','ls /Applications/Utilities','cat Desktop/riva.txt']
 for s in cli:cmd(s)
 g.visible('BuildVersion:',12,faces=(5,));g.shot('06-terminale');ok('Terminale: versione restituita da sw_vers e comando da disco; output delle altre pipeline salvato per il controllo indipendente')
 cmd('open -a Notes');g.visible('Tutte le note');g.type('Viaggio a Levante\nPrenotare il treno');g.key('ctrl-s');g.visible('Salvato',8);g.shot('07-note');g.key('ctrl-w',.5);g.key('f2');cmd('open -a Stickies');g.visible('Tutti i memo');g.type('Biglietti');g.key('ctrl-k');g.key('ctrl-s');g.visible('Salvato',8);g.shot('08-memo');g.key('ctrl-w',.5);g.key('f2');cmd('open -a "Disk Utility"');g.visible('Dispositivi e volumi');g.shot('09-utility-live');g.key('ctrl-w',.5)
 # Launch with a real USB mouse double-click on the live desktop icon.
 g.move(1205,250)
 for j in range(2):
  for mask in (1,0,1,0):g.hmp(f'mouse_button {mask}');time.sleep(.06)
  time.sleep(.65)
  if locate(g.frame(),'Introduzione'):break
 g.visible('Introduzione',10);g.shot('10-installa');g.key('ret');g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.key('ret');g.visible('Conferma l’installazione');g.key('spc');g.shot('11-conferma');g.key('ret');g.visible('Il disco è pronto',120,faces=(3,));g.shot('12-installato');g.key('ret');g.p.wait(timeout=13);assert g.p.returncode==0;ok('Installer aperto dalla scrivania via USB: destinazione, consenso esplicito, copia verificata e spegnimento')
class Post(Retail):
 initial='Elena'
with Post(None,W,E,'backup',disk='sata',boot_disk=True) as g:
 g.unlock()
 g.key('f2');g.visible('su console')
 g.cmd('open -a \"Disk Utility\"','Dispositivi e volumi')
 g.key('b');g.visible('Crea una copia locale');g.key('ret');g.visible('Copia locale salvata e verificata.',40)
 g.shot('13-copia-locale')
 g.key('f2');g.visible('su console');g.type('off');g.key('ret');g.p.wait(timeout=15);assert g.p.returncode==0
 ok('Sistema installato riavviato dal disco: Disk Utility crea la copia locale (TMCSNAP2) che i test offline verificano')
disk=W/'sata/system.raw';d=gpt(disk);assert d['esp']==d['src']==m['esp'] and d['rec']==m['rec'];assert m['install'] not in disk.read_bytes();ok('Lettura a VM spenta: GPT primaria e secondaria valide, ESP e sorgente locale identiche alla distribuzione installata, partizione Recovery con il file BaseSystem.dmg, identica alla sorgente verificata, nessun eseguibile dell’installer sul disco intero')
b=audit_bytes(d['data']);p='/Users/User/Documents/prova/';expected={'saluto.txt':b'Ciao Riviera\nAlba\n','sposta.txt':b'Ciao Riviera\n','conta':b'2\n','primo':b'Alba\n','ultimo':b'Alba\n','base':b'Preferences\n','dir':b'/Library\n','where':b'/usr/sbin/diskutil\n','chi':b'Elena\n','nome':b'Elena\n','luogo':b'/Users/User/Documents/prova\n','arch':b'x86_64\n','versione':b'2.4.3\n'}
for key,v in expected.items():assert b[p+key]==v,(key,b.get(p+key),v)
assert p+'vuoto' not in b and p+'cartella' not in b and p+'copia.txt' not in b and b['/Users/User/Desktop/riva.txt']==b'Riviera\n';assert b[p+'sha1'].split()[0]==hashlib.sha1(expected['sposta.txt']).hexdigest().encode() and b[p+'hash'].split()[0]==m['ks'].encode();assert b'ps' in b[p+'processi'] and b'Elena' not in b[p+'processi'] and b'2026' in b[p+'data'] and b'/dev/disk0' in b[p+'dischi'];assert set(b[p+'root'].decode().splitlines())==set('Applications Library System Users Volumes bin sbin usr'.split());ok('Dati TinyFS: byte esatti per redirezione, append, stdin, pipeline, mv/cp/rm/mkdir/rmdir, ls, pwd, whoami, ps/date, SHA-1 e SHA-256 del KernelCache uguali a hashlib')
assert b['/Users/User/Library/Application Support/Notes/1.txt']==b'Viaggio a Levante\nPrenotare il treno' and b['/Users/User/Library/Application Support/Stickies/1.txt']==b'Biglietti' and b['/Users/User/Library/Application Support/Stickies/1.txt.color']==b'2';ok('Note e Memo: contenuto modificato nelle app, colore e testo persistenti in Library/Application Support; nessun mock della UI')
assert b'/Library/Preferences/com.tinymacos.system.plist'.decode() in b and all(not v.lower().endswith(('.app','.zip','.iso','.c','.asm','.h')) for v in b);(E/'media.json').write_text(json.dumps(dict(iso=m['sha'],kernel=m['ks'],installed_esp=H(d['esp']),recovery_volume=H(d['rec']),basesystem_dmg=H(m['dmg']),dmg_bytes=len(m['dmg']),size=iso.stat().st_size),indent=2));ok('Library di sistema e utente effettive, nessun sorgente o manifesto installer nel volume personale')
