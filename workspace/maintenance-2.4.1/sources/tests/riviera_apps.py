"""Finder bundles, real file inspector, keyboard shell editing and separate productivity stores.
Run only on a copy of the dedicated riviera_live.py fixture. No guest debug services."""
import hashlib,json,os,shutil,struct,sys,time
from pathlib import Path
from retail_guest import Retail,locate
from riviera_media import gpt,audit_bytes

src=Path(sys.argv[1]).resolve();work=Path(sys.argv[2]).resolve();work.mkdir(parents=True,exist_ok=False)
d=Path(os.environ['TMC_BUILD']); disk=work/'apps.raw'; ev=work/'evidence';ev.mkdir(); results=[]
def ok(s):
    results.append(s); (ev/'apps.json').write_text(json.dumps(results,indent=2,ensure_ascii=False));print('OK:',s,flush=True)
state=gpt(src)
assert state['esp']==(d/'install.esp').read_bytes() and audit_bytes(state['data'])['/Users/User/Desktop/riva.txt']==b'Riviera\n'
del state
shutil.copyfile(src,disk);os.environ['DISK']=str(disk);os.environ.pop('RECOVERY_IMG',None)
raw=(d/'System-disk.vol').read_bytes();n,=struct.unpack_from('<I',raw,8);paths=[];dig={}
for i in range(n):
    par,isd,off,size,name=struct.unpack_from('<iIII48s',raw,16+i*64);q=paths[par]+'/'+name.split(b'\0')[0].decode() if i else '';paths.append(q)
    if q.startswith('/Applications/Notes.app/Contents/') and not isd:dig[q.rsplit('/',1)[1]]=hashlib.sha256(raw[off:off+size]).hexdigest()
del raw
class User(Retail):initial='Elena'
with User('/not-attached',work,ev,'apps',disk='vars',boot_disk=True) as g:
    g.unlock();g.key('f1',.42);x,y=g.visible('Preferiti',faces=(0,));side=(x-8,y-8,x+153,min(800,y+365));entries=(x+169,y+85,min(1280,x+770),min(800,y+344))
    def textclick(t,box=None,faces=(0,1,2,5,6,8)):
        a,b=g.visible(t,box=box,faces=faces);g.click(a+6,b+5)
    def applications():
        textclick('Applicazioni',side,(0,));g.visible('/Applications',faces=(0,));time.sleep(.34)
    def folder(s,expect):
        g.type(s);g.key('ret',.43);g.visible(expect)
    applications();g.type('notes');a,b=g.visible('Note',box=entries,faces=(0,));g.move(a+8,b+4);g.hmp('mouse_button 2');time.sleep(.12);g.hmp('mouse_button 0');g.visible('Mostra contenuto pacchetto');g.shot('22-pacchetto');textclick('Mostra contenuto pacchetto');g.visible('/Applications/Notes.app',faces=(0,));folder('contents','Info.plist');folder('info.plist','SHA-256');g.visible(dig['Info.plist'][:32],faces=(5,));g.visible(dig['Info.plist'][32:],faces=(5,));g.shot('23-manifesto');g.key('esc',.4);folder('macos','/Applications/Notes.app/Contents/MacOS');folder('notes','SHA-256');g.visible('cf fa ed fe',faces=(5,));g.visible(dig['Notes'][:32],faces=(5,));g.visible(dig['Notes'][32:],faces=(5,));g.shot('24-eseguibile');g.key('esc',.4);ok('Finder: menu contestuale sul bundle Note, navigazione in Contents, lettura di Info.plist e del Mach-O; entrambe le metà di ciascun SHA-256 sul display coincidono con i file del catalogo')
    applications();folder('notes','Tutte le note');g.visible('Prenotare il treno',faces=(5,));g.key('ctrl-n');g.type('Sentieri della costa\nStazione di Levanto');g.visible('Salvato',8);g.visible('2 note',faces=(0,));g.key('ctrl-f');g.type('Levanto');g.visible('1 nota',faces=(0,));g.shot('25-cerca-note');g.key('esc');g.visible('2 note',faces=(0,));textclick('Elimina',faces=(0,));g.visible('Eliminare questo elemento?');g.key('esc');g.visible('2 note',faces=(0,));g.key('ctrl-w',.55);ok('Avvio di Note dal bundle: il documento dell’installazione si riapre. Seconda nota autosalvata, ricerca nel testo e annullamento dell’eliminazione')
    g.key('f2');g.visible('su console');g.cmd('open -a Stickies','Tutti i memo');g.visible('Biglietti',faces=(5,));sx,sy=g.visible('Sulla scrivania',faces=(0,));assert g.frame().getpixel((sx+8,sy+94))==(204,228,250);textclick('Colore',faces=(0,));g.type(' e orari');g.visible('Salvato',8);assert g.frame().getpixel((sx+8,sy+94))==(248,210,220);g.shot('26-memo-disco');g.key('ctrl-w',.5);ok('Memo riaperto dal disco con il testo e il colore salvati; cambio colore e modifica con la tastiera USB')
    g.key('f2');g.cmd('open -a Notes','Tutte le note');g.key('ctrl-f');g.type('Stazione');g.visible('1 nota',faces=(0,));g.key('ret');g.visible('Stazione di Levanto',faces=(5,));textclick('Elimina',faces=(0,));g.visible('Eliminare questo elemento?');g.key('ret',.45);g.key('ctrl-f');g.key('esc');g.visible('1 nota',faces=(0,));g.key('ctrl-w',.5)
    g.key('f2');g.cmd('clear');g.type('sw_ve');g.key('tab');g.type('-productVersion > Documents/completato');g.key('ret',.35);g.cmd('echo x >> Documents/storia');g.key('up');g.key('ret',.4);g.cmd('nonesiste','Comando non trovato');g.cmd('echo $? > Documents/status');g.cmd('/bin/echo "con spazi" > "Documents/tempo libero"');g.cmd('off');g.p.wait(timeout=12);assert g.p.returncode==0
files=audit_bytes(gpt(disk)['data']);home='/Users/User/';sub=home+'Library/Application Support/'
assert files[sub+'Notes/1.txt']==b'Viaggio a Levante\nPrenotare il treno' and sub+'Notes/2.txt' not in files
assert files[sub+'Stickies/1.txt']==b'Biglietti e orari' and files[sub+'Stickies/1.txt.color']==b'3'
ok('A VM spenta: solo la nota eliminata con conferma è assente; memo, colore e documento originale sono integri nelle rispettive directory')
for k,v in {'completato':'2.4.1\n','storia':'x\nx\n','status':'127\n','tempo libero':'con spazi\n'}.items():assert files[home+'Documents/'+k]==v.encode(),(k,files.get(home+'Documents/'+k))
ok('Shell interattiva: completamento Tab, ripetizione dalla cronologia, stato 127, esecuzione per percorso assoluto e nome file con spazi controllati nei byte su disco')
