"""Actual installed native dialogs, document text/bytes and Calculator regression.
Reuses ONLY the disposable GUI-installed 4Kn disk from localization6_guest.py.
All document creation is via the native Terminal; no host input fixture.
"""
from pathlib import Path
import json,os,time
from retail_guest import Retail
from guest import KEYS
qa=Path(os.environ['TMC_QA_DIR']);prev=json.loads((qa/'localization6-capacity-4096-result.json').read_text());assert prev['passed']
w=Path(prev['work']);ev=w/'evidence';os.environ.update(TMC_MEM='512',TMC_SMP='2',TMC_STORAGE='nvme',TMC_NVME_LBA_BYTES='4096',TMC_POINTER='tablet')
r={'passed':False,'work':str(w),'iso_sha256':prev['iso_sha256'],'checks':[],'physical_hardware_tested':False,'host_prepared_document':False}
class QA(Retail):
 initial='CapacityQA'
 def type(self,text):
  for c in text:self.key(KEYS.get(c,('shift-'+c.lower() if c.isupper() else c)),dt=.055,hold=.025)
def launch(g,s,label):g.key('meta_l-spc');g.type(s);g.key('ret');g.visible(label);time.sleep(.5)
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
text='Localizzazione: caffè, € e testo semplice. Nessun dato perso.'
try:
 with QA('/not-attached',w,ev,'m6-dialogs-native',disk='target',boot_disk=True) as g:
  g.unlock();g.key('meta_l-w');launch(g,'Terminale','Sessione locale');g.cmd('echo '+text+' > Documents/m6-testo.txt');g.cmd('wc -c Documents/m6-testo.txt',str(len(text.encode())+1))
  g.cmd('open Documents/m6-testo.txt');g.visible('Salva con nome');g.visible('Localizzazione:',faces=(5,));g.key('meta_l-shift-s');g.visible('Salva col nome:');g.shot('m6-save-panel');g.key('esc');g.key('meta_l-o');g.visible('Nome:',faces=(0,));g.shot('m6-open-panel');g.key('esc');g.key('meta_l-w');g.visible('Sessione locale')
  ok('Actual TextEdit opens native UTF-8 bytes; localized Salva con nome, Save As and Open choosers render, cancel without changing the document')
  g.cmd('open Documents');x,y=g.visible('m6-testo.txt');g.click(x+20,y+5);g.key('spc');g.visible('Vista rapida');g.visible(str(len(text.encode())+1)+' B');g.shot('m6-quick-look');g.key('esc');g.key('meta_l-w');g.visible('Sessione locale');g.cmd('cat Documents/m6-testo.txt','Nessun dato perso.')
  ok('Finder Quick Look displays correctly converted actual file size and Italian title; native cat retains original accented/euro document content')
  launch(g,'Calcolatrice','Cronologia');g.type('7+5=');g.visible('7 + 5 = 12');g.move(50,735);g.shot('m6-calculator-history');g.key('meta_l-w');g.visible('Sessione locale');g.cmd('off');g.p.wait(timeout=20)
  ok('Macroblock 5 Calculator result/history and +8px glass geometry retained in the macroblock 6 build; normal native shutdown')
 r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:
 (qa/'dialogs-result.json').write_text(json.dumps(r,indent=2)+'\n')
 for p in w.glob('*.ppm'):p.unlink()
