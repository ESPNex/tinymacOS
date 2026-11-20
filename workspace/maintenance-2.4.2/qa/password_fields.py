#!/usr/bin/env python3
"""Compare actual setup and locked-login fields, including maximum input length."""
import json,os,sys,tempfile,time
from pathlib import Path
import numpy as np
sys.path.insert(0,'/home/user/tinymacOS/tests')
from retail_guest import Retail
w=Path(__file__).resolve().parents[1];e=w/'evidence'
password='secret-'+'x'*57
results={}
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD='/var/tmp/tmc-maintenance-242',TMC_USB_LAYOUT='standard')
def band(im,box):
 a=np.asarray(im)[box[1]:box[3],box[0]:box[2],:3];mask=np.all(a>=220,axis=2);best=0
 for row in mask:
  run=0
  for p in row:
   run=run+1 if p else 0;best=max(best,run)
 return int(best)
for tag,iso in [('before','/var/tmp/tmc-maintenance-241/tinymacOS-2.4.1-x86_64.iso'),('after','/var/tmp/tmc-maintenance-242/tinymacOS-2.4.2-x86_64.iso')]:
 run=Path(tempfile.mkdtemp(prefix='tmc-password-242-',dir='/var/tmp'));samples=[]
 with Retail(Path(iso),run,e,'password-'+tag,live=True) as g:
  g.type('Tester');g.key('tab');g.shot(tag+'-password-0')
  for i,c in enumerate(password):
   g.key('minus' if c=='-' else c,dt=.04)
   if i+1 in (1,16,64):
    time.sleep(.15);im=g.snap();g.shot(tag+'-password-'+str(i+1));samples.append({'screen':'setup','characters':i+1,'max_near_white_run':band(im,(513,473,767,507))})
  for i in range(3):
   time.sleep(.45);g.shot(tag+'-password-blink-'+str(i))
  g.key('tab');g.type(password);g.key('ret');g.visible('Preferiti',40)
  g.key('ctrl-shift-q');g.visible('Tester',15);g.shot(tag+'-locked-empty')
  g.type(password);im=g.snap();g.shot(tag+'-locked-password')
  samples.append({'screen':'locked','characters':64,'max_near_white_run':band(im,(518,613,730,647))})
  g.key('ret');g.visible('Preferiti',40);g.shot(tag+'-reauthenticated')
  if tag=='after':assert all(s['max_near_white_run']<=15 for s in samples),samples
 results[tag]={'samples':samples,'64_character_account_and_reauthentication':True}
(e/'password-fields.json').write_text(json.dumps(results,indent=2))
print('PASS: 0/1/16/64-character password screenshots, blink phases, account creation and locked-login reauthentication; no long near-white run in new field samples.')
