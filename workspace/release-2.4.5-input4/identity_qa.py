#!/usr/bin/env python3
"""Audit actual ISO identity and drawn retail labels; outputs external except small report."""
import hashlib,json,os,plistlib,sys,tempfile,time,subprocess,struct
from pathlib import Path
home=Path(__file__).resolve().parents[2];sys.path.insert(0,str(home/'tinymacOS/tests'))
from retail_guest import Retail
from riviera_media import medium
out=Path(tempfile.mkdtemp(prefix='tmc245-identity-',dir='/var/tmp'));ev=out/'evidence';ev.mkdir()
iso=Path('/var/tmp/tmc245-input4-final/tinymacOS-2.4.5-OpenCore-Installer-x86_64.iso')
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD=str(iso.parent),TMC_POINTER='tablet',TMC_SMP='2')
checks=[];record={'passed':False,'version':'2.4.5','build':'21B9','codename':'Levante','iso_sha256':hashlib.sha256(iso.read_bytes()).hexdigest(),'evidence_external':str(ev)}
def catalog(raw):
 import struct
 b=raw[:-32];assert hashlib.sha256(b).digest()==raw[-32:];_,n,step=struct.unpack_from('<8sII',b);paths=[];data={}
 for j in range(n):
  parent,d,off,size,name=struct.unpack_from('<iIII48s',b,16+j*step);name=name.split(b'\0')[0].decode();p=(paths[parent]+'/'+name) if j else '';paths.append(p);data[p]=None if d else b[off:off+size]
 return data
try:
 font=(home/'tinymacOS/assets/type.tfn').read_bytes();nf,ng=struct.unpack_from('<II',font,4);codes=list(struct.unpack_from(f'<{ng}I',font,12));assert 0x20ac in codes
 for cp in map(ord,'àèéìòù€£§ç°'):
  assert cp in codes
 medium(iso,out/'media',checks.append)
 env={**os.environ,'PATH':'/var/tmp/tmc/tools/root/usr/bin:'+os.environ['PATH'],'LD_LIBRARY_PATH':'/var/tmp/tmc/tools/root/usr/lib/x86_64-linux-gnu'}
 subprocess.run(['xorriso','-osirrox','on','-indev',str(iso),'-extract','/START.TXT',str(out/'start.txt')],env=env,check=True,capture_output=True)
 start=(out/'start.txt').read_text();assert '21B9' in start and '21B8' not in start and 'Nessun selettore o fallback US' in start
 # medium's independent mcopy readers have saved exactly the tested payloads.
 for name in ['opt.fat.vol','disk.esp.vol']:
  c=catalog((out/'media'/name).read_bytes());version=plistlib.loads(c['/System/Library/CoreServices/SystemVersion.plist']);assert version=={'ProductName':'tinymacOS','ProductVersion':'2.4.5','ProductBuildVersion':'21B9'}
  for p,b in c.items():
   if p.endswith('/Contents/Info.plist') and '/Extensions/' not in p:assert plistlib.loads(b)['CFBundleShortVersionString']=='2.4.5',p
   if b:assert b'2.4.4' not in b and b'21B6' not in b,p
  about=c['/System/Library/CoreServices/About This Computer.app/Contents/MacOS/About This Computer'];assert b'LEVANTE' in about and b'RIVIERA' not in about
  if 'opt' in name:
   installer=c['/Applications/Install tinymacOS.app/Contents/MacOS/Install tinymacOS'];assert b'LEVANTE' in installer and b'RIVIERA' not in installer
 assert b'tinymacOS Recovery 2.4.5 (21B9)' in (out/'media/recovery.efi').read_bytes()
 assert b'Benvenuti in tinymacOS 2.4.5 Levante.' in (out/'media/outer.cache').read_bytes()
 with Retail(iso,out,ev,'identity',live=True) as g:
  g.enroll(name='Levante');g.key('f2');g.visible('Sessione locale');g.cmd('sw_vers','ProductVersion:    2.4.5');g.visible('BuildVersion:      21B9');g.cmd('uname -a','tinymacOS levante 2.4.5 21B9 x86_64');g.shot('terminal-version')
  g.cmd('open -a "About This Computer"');g.visible('Versione 2.4.5');g.visible('LEVANTE');g.shot('about-levante');g.key('ctrl-w');g.key('f11');g.visible('Introduzione');g.visible('LEVANTE');g.visible('tinymacOS 2.4.5');g.shot('installer-levante')
 record.update(passed=True,media_checks=checks,all_app_plists=True,system_version_plists=True,recovery_binary_version=True,welcome_version=True,terminal_version=True,drawn_about_installer_levante=True,old_21B6_build_strings_absent_in_system_volumes=True)
finally:(Path(__file__).parent/'identity-result.json').write_text(json.dumps(record,indent=2,ensure_ascii=False)+'\n')
print('PASS actual ISO 2.4.5/21B9 metadata, Recovery, command output and drawn LEVANTE labels; screenshots:',ev)
