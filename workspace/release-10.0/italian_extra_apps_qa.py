#!/usr/bin/env python3
"""Italian typing in the remaining shared-text clients: Notes, Stickies, Calendar,
Settings search and desktop search. Actual USB keyboard, i8042 absent.
"""
import hashlib,json,os,sys,tempfile,time
from pathlib import Path
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'tinymacOS/tests'))
from retail_guest import Retail
iso=Path(os.environ['TMC_TEST_ISO']);w=Path(tempfile.mkdtemp(prefix='tmc10-extra-apps-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_MEM='512',TMC_POINTER='mouse',TMC_TEST_FIRMWARE='uefi',TMC_TEST_INPUT='xhci-hub-kbd')
r={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'checks':[]}
def digest(s):return hashlib.sha256(s.encode()).hexdigest()
def passed(s):r['checks'].append(s);print('PASS',s,flush=True)
try:
 with Retail(iso,w,ev,'extra-apps',live=True,launcher=root/'tinymacOS/tools/run-input-qa.sh') as g:
  assert 'i8042' not in g.hmp('info qtree');g.enroll('Italiano');g.key('f2');g.visible('Sessione locale')
  for app,heading in [('Notes','Tutte le note'),('Stickies','Tutti i memo')]:
   g.cmd('open -a '+app);g.visible(heading);g.type('àèéìòù€');g.key('left');g.key('backspace');g.key('delete');g.type(' €@#[]{}');body='àèéìò €@#[]{}'
   g.key('ctrl-s');time.sleep(.4);g.shot(app.lower()+'-italian');g.visible('€',faces=(5,));g.key('ctrl-w');g.key('f2')
   g.cmd('shasum -a 256 "Library/Application Support/'+app+'/1.txt"',digest(body));passed(app+': Italian accents/AltGr/euro, UTF-8 caret/delete and saved bytes SHA-256')
  g.key('f7');g.visible('Novembre');g.key('ret');g.type('Città è già qui €@#');g.key('ret');time.sleep(.4);g.shot('calendar-italian');g.key('ctrl-w');g.key('f2')
  g.cmd('shasum -a 256 Documents/Agenda/2026-11-18.txt',digest('Città è già qui €@#'));passed('Calendar memo: UTF-8 bytes verified by another process')
  g.key('f4');x,y=g.visible('Cerca',faces=(0,));g.click(x+10,y+6);g.type('àèéìòù€');g.visible('€',faces=(0,));g.shot('settings-italian-search')
  for _ in range(7):g.key('backspace',.04)
  g.type('Aspetto');g.visible('Aspetto');g.key('esc');g.key('ctrl-w');passed('Settings search accepts accented text and removes whole UTF-8 scalars')
  g.key('f2');g.cmd('echo -n "àè€" > Documents/città.txt');g.key('ctrl-spc');g.type('città');g.visible('città.txt');g.shot('desktop-italian-search');g.key('esc');passed('Desktop search finds an accented filename with Italian physical keys')
  r['passed']=True
finally:(Path(__file__).parent/'italian-extra-apps.json').write_text(json.dumps(r,indent=2,ensure_ascii=False)+'\n')
print('PASS remaining Italian text clients',w)
