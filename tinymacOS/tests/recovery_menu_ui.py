"""Recovery menu bar acceptance on the actual ISO, not a UI mock."""
from pathlib import Path
import os,time,json,struct,hashlib,tempfile
import numpy as np
from retail_guest import Retail
from boot_recovery import RecoveryKey
import pixels
root=Path(__file__).resolve().parents[1];base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO'])
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
w=Path(tempfile.mkdtemp(prefix='tmc1021-recovery-menu-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',OCR_ROOT='/var/tmp/tmc/tools/root',TMC_MEM='512',TMC_POINTER='tablet')
r={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'physical_hardware_tested':False,'checks':[]}
class Recovery(RecoveryKey,Retail):recovery_text='Ripristina da una copia locale'
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
def menu(g,title,item):
 x,y=g.visible(title,box=(0,0,800,34));g.click(x+12,y+8);x,y=g.visible(item,box=(0,34,900,240));g.click(x+20,y+8)
try:
 with Recovery(iso,w,ev,'menu',live=True) as g:
  g.click(23,16);g.visible('Riavvia',box=(0,34,300,180));g.shot('apple-menu');x,y=g.visible('Riavvia',box=(0,34,300,180));g.click(x+20,y+8);g.visible('Riavviare il computer?');g.key('esc');g.visible('Ripristina da una copia locale');ok('Apple menu opens a real restart confirmation; Escape cancels without reboot')
  menu(g,'Aiuto','Info sul Recupero');g.visible('Versione 10.2.5');g.shot('info-menu-only');g.key('ret');ok('Version is available only on explicit menu request, not in the home window')
  g.key('f10');g.visible('Terminale',box=(0,34,900,240));assert pixels.locate(g.snap().crop((0,34,950,330)),'FileVault',threshold=.98) is None;g.key('down');g.key('ret');g.visible('Sessione locale');g.shot('keyboard-menu-terminal');ok('F10, Down and Enter activate the native Terminal through Utility')
  menu(g,'File','Chiudi finestra');g.visible('Recupero tinymacOS',box=(0,0,800,34));ok('File > Close closes the native client and restores the Recovery app menu')
  menu(g,'Utility','Terminale');g.visible('Sessione locale');menu(g,'Finestra','Riduci a icona');g.visible('Recupero tinymacOS',box=(0,0,800,34));menu(g,'Finestra','Terminale');g.visible('Sessione locale');ok('Window menu minimizes and restores the app without needing a Dock')
  menu(g,'File','Chiudi finestra');menu(g,'Utility','Inizializza i dati utente');g.visible('Nessun disco disponibile.');g.key('ret');ok('Destructive utility refuses to run without an eligible disk')
  im=np.asarray(g.snap());assert tuple(im[760,640])==(3,3,3);assert tuple(im[10,800])==(36,36,36);g.shot('clean-home-final');ok('No Dock or Control Center is introduced; opaque menu bar remains present')
 r['passed']=True
finally:(base/'recovery-menu-result.json').write_text(json.dumps(r,indent=2)+'\n')
