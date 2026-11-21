#!/usr/bin/env python3
"""Real ISO UI: Italian login/password, Terminal, TextEdit, Finder and locking.
Hashes are produced by the guest's shasum command, not inferred from screenshots.
"""
import hashlib,json,os,sys,tempfile,time
from pathlib import Path
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'tinymacOS/tests'))
from retail_guest import Retail
iso=Path(os.environ['TMC_TEST_ISO']);profile=os.environ.get('TMC_TEST_INPUT','ps2');firmware=os.environ.get('TMC_TEST_FIRMWARE','uefi')
w=Path(tempfile.mkdtemp(prefix='tmc10-italian-apps-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_MEM='512',TMC_POINTER='mouse')
record={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'profile':profile,'firmware':firmware,'work':str(w),'checks':[]}
def passed(s):record['checks'].append(s);print('PASS',s,flush=True)
def digest(s):return hashlib.sha256(s.encode()).hexdigest()
password='città-è-é-€-285'
try:
 with Retail(iso,w,ev,'italian',live=True,launcher=root/'tinymacOS/tools/run-input-qa.sh') as g:
  qtree=g.hmp('info qtree');record['i8042_present']='i8042' in qtree
  if profile.endswith('-kbd'):assert not record['i8042_present']
  g.enroll('Niccolò',password);passed('Accented account name and UTF-8 password enroll through actual native key events')
  g.key('f2');g.visible('Sessione locale')
  phrase="àèéìòù€ @#[]{} \\|<> /?;:_=+*£"
  # Shell metacharacters are quoted so the production parser preserves them.
  g.cmd('echo -n "'+phrase.replace('\\','\\\\')+'" > Documents/italiano.txt')
  # No escape processing is needed for these characters in a double-quoted argument.
  expected=digest(phrase)
  g.cmd('shasum -a 256 Documents/italiano.txt',expected)
  passed('Italian accents/euro/AltGr/punctuation round trip through shell and filesystem, SHA-256 verified')
  g.key('f3');g.visible('Testo semplice');g.type('àèéìòù€');g.key('left');g.key('backspace');g.key('delete')
  g.type(' €@#[]{}');body='àèéìò €@#[]{}'
  g.key('ctrl-shift-s');g.type('città.txt');g.key('ret');time.sleep(.6);g.shot('textedit-italian');g.visible('€',faces=(5,));g.visible(body,faces=(5,))
  g.key('ctrl-w');g.key('f2');g.cmd('shasum -a 256 Documents/città.txt',digest(body))
  passed('TextEdit Unicode insertion, left/backspace/delete and accented Save As filename, exact UTF-8 bytes verified')
  g.key('f1');g.visible('Preferiti');g.key('ctrl-n');g.visible('Nuova cartella');g.type('Città');g.key('ret');time.sleep(.4)
  g.key('f2');g.cmd('ls','Città');passed('Finder Italian folder name is created and read by another process')
  g.key('ctrl-shift-q');g.visible('Niccolò');g.type(password);g.key('ret');g.visible('Preferiti',35)
  passed('Italian password authenticates again after lock, no ASCII-only path')
  g.shot('italian-desktop');record['passed']=True
finally:(Path(__file__).parent/f'italian-apps-{firmware}-{profile}.json').write_text(json.dumps(record,indent=2,ensure_ascii=False)+'\n')
print('PASS Italian apps',firmware,profile,w)
