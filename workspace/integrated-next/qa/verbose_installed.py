#!/usr/bin/env python3
"""Real installer + installed Terminal setting round trips; disposable SATA only."""
import hashlib,json,os,sys,tempfile,time,struct
from pathlib import Path
sys.path.insert(0,'/home/user/tinymacOS/tests')
from retail_guest import Retail
from riviera_media import medium,gpt
w=Path(tempfile.mkdtemp(prefix='tmc-verbose-installed-',dir='/var/tmp'));e=Path(__file__).resolve().parents[1]/'evidence/verbose-installed';e.mkdir(exist_ok=True)
iso=Path('/var/tmp/tmc-integrated-system/tinymacOS-2.4.3-x86_64.iso')
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD=str(iso.parent),TMC_USB_LAYOUT='standard',TMC_POINTER='mouse')
checks=[];record={'passed':False,'scope':'native installed UEFI verbose preference, not BIOS installer support'}
def media_state():
 disk=w/'sata/system.raw';layout=gpt(disk);rec=layout['rec'];head=struct.unpack_from('<HBHBHHBHHHII',rec,11)
 reserved=struct.unpack_from('<H',rec,14)[0];fat=struct.unpack_from('<H',rec,22)[0];entries=struct.unpack_from('<H',rec,17)[0];root=reserved+2*fat;data=root+(entries*32+511)//512
 cfg=None
 for j in range(root*512,(root+(entries*32+511)//512)*512,32):
  if rec[j:j+11]==b'BOOTCFG BIN':
   cluster=struct.unpack_from('<H',rec,j+26)[0];off=(data+cluster-2)*512;cfg=rec[off:off+512];absolute=(layout['parts'][2][0]*512)+off;break
 assert cfg and cfg[:8]==b'TMCBOOT1' and cfg[8:12]==struct.pack('<I',1) and hashlib.sha256(cfg[:16]).digest()==cfg[16:48]
 return layout,cfg,absolute
def toggle(guest,value):
 guest.cmd('bootverbose '+value,'Preferenza verificata su disco');guest.cmd('bootverbose status','Prossimo avvio: verbose '+('attivo' if value=='on' else 'disattivo'));guest.shot('setting-'+value)
try:
 catalog=medium(iso,w/'media',checks.append)
 with Retail(iso,w,e,'install',disk='sata',live=False) as g:
  g.enroll(name='Verbose');g.move(1205,250)
  for down in (True,False,True,False):
   g.req('input-send-event',events=[{'type':'btn','data':{'button':'left','down':down}}]);time.sleep(.055)
  g.visible('Introduzione',15);g.key('ret');g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.key('ret');g.key('spc');g.key('ret');g.visible('Il disco è pronto',120,faces=(3,));g.shot('installed');g.key('ret');g.p.wait(timeout=15);assert g.p.returncode==0
 layout,config,off=media_state();assert layout['esp']==catalog['esp'] and layout['rec']==catalog['rec'] and config[12:16]==bytes(4)
 class Installed(Retail):initial='Verbose'
 with Installed('/not-attached',w,e,'disk-on',disk='sata',boot_disk=True) as g:
  g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('bootverbose status','Verbose avvio corrente: disattivo');toggle(g,'on')
 layout,config,_=media_state();assert layout['esp']==catalog['esp'] and config[12:16]==struct.pack('<I',1);assert layout['src']==catalog['esp']
 # The sole changed Recovery sector must be BOOTCFG.BIN, not the DMG/EFI/FAT.
 recovery_expected=bytearray(catalog['rec']);relative=off-layout['parts'][2][0]*512;recovery_expected[relative:relative+512]=config;assert layout['rec']==recovery_expected
 with Installed('/not-attached',w,e,'disk-off',disk='sata',boot_disk=True) as g:
  g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('bootverbose status','Verbose avvio corrente: attivo');g.shot('reboot-enabled');g.cmd('bootverbose log','[boot] tinymacOS native kernel');g.shot('boot-log');toggle(g,'off')
 layout,config,_=media_state();assert layout['rec']==catalog['rec'] and layout['esp']==catalog['esp']
 with Installed('/not-attached',w,e,'disk-final',disk='sata',boot_disk=True) as g:
  g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('bootverbose status','Verbose avvio corrente: disattivo');g.shot('reboot-disabled')
 record.update(passed=True,media_audit=checks,installer=True,boot_without_iso=True,off_on_off_reboots=True,esp_and_restore_payload_unchanged=True,recovery_only_config_sector_modified=True,config_sha256_verified=True)
finally:(e/'result.json').write_text(json.dumps(record,indent=2,ensure_ascii=False))
print('PASS installed UEFI off -> Terminal on -> reboot verbose -> Terminal off -> reboot quiet; sealed ESP and recovery payload unchanged')
