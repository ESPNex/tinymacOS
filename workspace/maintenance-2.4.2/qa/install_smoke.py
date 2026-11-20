#!/usr/bin/env python3
"""Install onto a disposable QEMU SATA disk, boot without ISO and log in."""
import json,os,sys,tempfile,time
from pathlib import Path
sys.path.insert(0,'/home/user/tinymacOS/tests')
from retail_guest import Retail
from riviera_media import medium,gpt
w=Path(tempfile.mkdtemp(prefix='tmc-install-242-',dir='/var/tmp'));e=Path(__file__).resolve().parents[1]/'evidence'
iso=Path('/var/tmp/tmc-maintenance-242/tinymacOS-2.4.2-x86_64.iso')
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD=str(iso.parent),TMC_USB_LAYOUT='standard',TMC_POINTER='mouse')
checks=[];catalog=medium(iso,w/'media',checks.append)
with Retail(iso,w,e,'install-242',disk='sata',live=False) as g:
 g.enroll(name='Tester');g.move(1205,250)
 for down in (True,False,True,False):
  g.req('input-send-event',events=[{'type':'btn','data':{'button':'left','down':down}}]);time.sleep(.055)
 g.visible('Introduzione',15);g.key('ret');g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.key('ret');g.key('spc');g.key('ret');g.visible('Il disco è pronto',120,faces=(3,));g.shot('installed');g.key('ret');g.p.wait(timeout=15);assert g.p.returncode==0
class Installed(Retail):initial='Tester'
with Installed('/not-attached',w,e,'disk-242',disk='sata',boot_disk=True) as g:
 g.unlock();assert not any(str(iso) in str(b) for b in g.req('query-block'))
 g.key('f2');g.visible('Sessione locale',15);g.cmd('sw_vers');g.visible('2.4.2',15,faces=(5,));g.shot('installed-version-242')
layout=gpt(w/'sata/system.raw');assert layout['esp']==catalog['esp'] and layout['rec']==catalog['rec']
(e/'install-smoke.json').write_text(json.dumps({'passed':True,'retail_installer':True,'disposable_sata_disk':True,'mouse_profile':'relative USB','post_install_boot_without_iso':True,'new_nvram_state':False,'installed_login':True,'sw_vers':'2.4.2','installed_esp_and_recovery_match_media':True,'media_checks':checks},indent=2,ensure_ascii=False))
print('PASS: media audit, actual installer, no-CD SATA boot/login, installed version 2.4.2, ESP/Recovery byte comparisons.')
