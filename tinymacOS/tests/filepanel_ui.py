"""Native login and TextEdit/Finder file panel acceptance on a disposable live ISO.
Set TMC_STAGE_ISO, TMC_FONT, TOOLROOT, TMC_BUILD and an external TMC_QA_DIR.
"""
from pathlib import Path
import os,sys,time,json,hashlib
import numpy as np
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tests'))
from retail_guest import Retail
import pixels,struct
raw=Path(os.environ.get('TMC_FONT',str(root/'assets/type.tfn'))).read_bytes()
pixels.b=raw;pixels.nf,pixels.ng=struct.unpack_from('<II',raw,4)
pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',raw,12))}
pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
b=Path(os.environ.get('TMC_QA_DIR','/var/tmp/tmc-filepanel-qa'));b.mkdir(parents=True,exist_ok=True);ev=b/'screenshots';ev.mkdir(exist_ok=True)
os.environ.update(TOOLROOT=os.environ.get('TOOLROOT','/var/tmp/tmc/tools/root'),OCR_ROOT=os.environ.get('OCR_ROOT','/var/tmp/tmc/tools/root'),TMC_BUILD=os.environ.get('TMC_BUILD','/var/tmp/tmc-current'),TMC_MEM='512',TMC_POINTER='tablet',PYTHONDONTWRITEBYTECODE='1')
iso=Path(os.environ.get('TMC_STAGE_ISO',str(b/'tinymacOS-10.2-x86_64.iso')));r={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'checks':[]}
def ok(t):r['checks'].append(t);print('PASS',t,flush=True)
def caret(g,x,y):
 states=[]
 for _ in range(24):
  im=np.asarray(g.snap());states.append(int(np.sum(np.all(im[y+8:y+25,x+16,:]==255,axis=1))))
  if states[-1]>13:g.snap().save(ev/'01-login-password-caret.png')
  time.sleep(.08)
 assert max(states)>=16 and min(states)<3,states
 return states
def origin(g):
 x,y=g.visible('Testo semplice',faces=(0,));return x-641,y-18

def filename(g,X,Y,name):
 g.click(X+300,Y+395-12);g.type(name)
def accept(g,X,Y):g.click(X+690,Y+452-6)
try:
 with Retail(iso,b,ev,'filepanel',live=True) as g:
  g.type('Elena');g.key('tab');time.sleep(.4);assert int(np.sum(np.all(np.asarray(g.snap())[481:498,529,:]==255,axis=1)))<3;g.type('l');g.key('left');r['password_caret_samples']=caret(g,513,473);g.key('right');g.type('ocal-test-28');g.key('left');g.type('X');g.key('delete');g.key('backspace');g.key('end');g.type('85');g.key('tab');g.type('local-test-285');g.key('ret');g.visible('Preferiti',35);ok('Setup password caret hidden until typing, then blinks without plaintext; UTF8-safe caret editing authenticates the resulting password')
  g.app('f3','Testo semplice');X,Y=origin(g);g.type('HELLO19');g.key('ctrl-s');g.visible('Salva documento');time.sleep(.5)
  assert pixels.locate(g.snap().crop((X,Y-42,X+750,Y)),'Untitled.txt',faces=(2,),threshold=.94) is None
  assert pixels.locate(g.snap(),'Testo semplice',faces=(0,),threshold=.98) is None
  assert pixels.locate(g.snap().crop((0,34,X+10,700)),'KernelCollections',faces=(0,),threshold=.94) is not None
  ok('Only the file panel has no extra window frame; background Finder remains visible and open')
  g.snap().save(ev/'02-salva-documento.png')
  g.app('f2','Terminale');g.cmd('echo APP-ATTIVA','APP-ATTIVA');g.key('f3');g.visible('Salva documento');ok('Other apps remain usable while the file panel is open; switching back preserves the pending dialog and document')
  # Create a real folder in the sheet; no shell fixture is used.
  g.click(X+80,Y+447);g.visible('Nuova cartella');g.type('Prova19');g.key('ret');g.visible('Cartella vuota');ok('Save panel creates and enters a real user folder')
  g.click(X+300,Y+383);g.type('UI19.txt');g.click(X+690,Y+446);g.visible('Salvato nella sessione live');g.visible('KernelCollections');ok('First Save opens native chooser and writes the selected path')
  g.app('f2','Terminale');g.cmd('cat /Users/User/Documents/Prova19/UI19.txt','HELLO19');g.cmd('sw_vers -productVersion','10.2');ok('Saved file content and version verified through real Terminal/filesystem')
  g.app('f3','Testo semplice');X,Y=origin(g);g.key('ctrl-n');g.key('ctrl-o');g.visible('Apri documento');g.key('ctrl-l');g.type('/Users/User/Documents/Prova19');g.key('ret');g.visible('UI19.txt');g.snap().save(ev/'03-apri-documento.png')
  # Removed action stays inert, while shared filesystem navigation still works.
  assert pixels.locate(g.snap(),'Mostra nel Finder',faces=(0,1,2,3)) is None
  g.click(X+680,Y+32);g.visible('Apri documento');g.snap().save(ev/'04-dialogo-senza-finder.png');ok('Finder button removed; former target cannot launch another window')
  g.key('down');g.key('ret');g.visible('HELLO19');ok('Open chooser browses folders and loads the selected file into TextEdit')
  # All three user-facing Open entry points must use the same modern panel.
  g.click(X+98,Y+22);g.move(1100,650);g.visible('Apri documento');g.visible('Preferiti');g.key('esc');g.visible('HELLO19');ok('Toolbar Apri opens the modern chooser; cancel preserves the loaded document')
  mx,my=g.visible('File',box=(100,0,480,33));g.click(mx+8,my+6);mx,my=g.visible('Apri documento');g.click(mx+35,my+6);g.visible('Apri documento');g.visible('Nome file:');g.key('esc');g.visible('HELLO19');ok('File menu Apri documento opens the same modern chooser')
  g.type('DIRTY19');g.click(X+98,Y+22);g.visible('Aprire un altro documento?');g.snap().save(ev/'05a-conferma-non-salvato.png');cx,cy=g.visible('Annulla');g.click(cx+20,cy+5);g.visible('DIRTY19');ok('Mouse Cancel in unsaved-changes confirmation preserves the document')
  g.key('ctrl-o');g.visible('Aprire un altro documento?');cx,cy=g.visible('Non salvare');g.click(cx+25,cy+5);g.visible('Apri documento');g.visible('Nome file:');g.snap().save(ev/'05b-apri-moderno-dopo-conferma.png');g.key('esc');g.visible('DIRTY19');g.key('ctrl-s');g.visible('Salvato nella sessione live');ok('Dirty Ctrl+O confirmation opens modern chooser; canceling that chooser still preserves unsaved bytes')
  g.key('ctrl-shift-s');g.visible('Salva documento');g.click(X+690,Y+446);g.visible('Sostituire il documento?');g.snap().save(ev/'05-conferma-sovrascrittura.png');g.key('esc');g.key('esc');g.visible('HELLO19');ok('Overwrite confirmation can be canceled without losing the document')
  g.key('ctrl-shift-s');g.visible('Salva documento');g.click(X+300,Y+383);g.type('/System/Nope19.txt');g.click(X+690,Y+446);g.visible('La destinazione non e scrivibile.');g.key('esc');ok('Readonly destinations rejected in the actual save sheet')
  g.app('f2','Terminale');g.cmd('cat /Users/User/Documents/Prova19/UI19.txt','DIRTY19');ok('Canceled and rejected saves leave existing file bytes unchanged')
  # Lock via real Apple menu, then prove login caret and password editing.
  g.click(23,16);px,py=g.visible('Blocca schermo');g.click(px+35,py+6);g.visible('Inserisci la password');time.sleep(.4);assert int(np.sum(np.all(np.asarray(g.snap())[621:638,534,:]==255,axis=1)))<3;g.type('l');g.key('left');r['locked_caret_samples']=caret(g,518,613);g.key('right');g.type('ocal-test-285');g.key('ret');g.visible('Terminale',35);ok('Locked-session password caret and unlock work')
  r['passed']=True
finally:(b/'filepanel-result.json').write_text(json.dumps(r,indent=2)+'\n')
