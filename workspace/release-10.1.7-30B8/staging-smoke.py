#!/usr/bin/env python3
"""Development acceptance, NOT a release gate or a Linux-driver-port receipt."""
import hashlib,json,os,sys,tempfile,time
from pathlib import Path
q=Path(__file__).resolve().parent;home=q.parents[1];sys.path.insert(0,str(home/'tinymacOS/tests'))
from retail_guest import Retail
from boot_recovery import RecoveryKey
fw=sys.argv[1];assert fw in ['uefi','bios'];iso=Path(os.environ.get('TMC_STAGE_ISO','/var/tmp/tmc-10.1.7/tinymacOS-10.1.7-x86_64.iso'));w=Path(tempfile.mkdtemp(prefix='tmc1017-stage-'+fw+'-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',OCR_ROOT='/var/tmp/tmc/tools/root',TMC_BUILD=os.environ.get('TMC_STAGE_BUILD',str(iso.parent/'linux-config-v10/build')),TMC_BIOS_ISO=str(iso),TMC_POINTER='mouse',TMC_MEM='512')
launcher=home/'tinymacOS/tools/run-bios-iso.sh' if fw=='bios' else None
r={'passed':False,'firmware':fw,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'linux_hcd_port':False,'physical_hardware_tested':False,'checks':[]}
def ok(s):r['checks'].append(s);print('PASS',fw,s,flush=True)
try:
 with Retail(iso,w,ev,'desktop',live=True,launcher=launcher) as g:
  g.enroll('Sviluppo');g.key('f2');g.visible('Sessione locale');g.cmd('bootverbose status','Verbose avvio corrente: disattivo');g.visible('Preferenza persistente non disponibile');g.shot('quiet-default');ok('Actual OpenCore-booted live desktop reports verbose OFF; no persistent setting is claimed for diskless live media')
  g.cmd('sw_vers','ProductVersion:    10.1.7');g.visible('BuildVersion:      30B8');ok('Terminal identity 10.1.7 / 30B8')
  def command(s,status,index):
   g.cmd(s);time.sleep(.25);g.cmd('echo D'+str(index)+'-$?','D'+str(index)+'-'+str(status));ok(s+' -> '+str(status))
  command('mkdir Desktop/Vuota',0,1);command('rmdir Desktop/Vuota/',0,2);command('ls Desktop/Vuota',1,3)
  command('mkdir Desktop/Albero Desktop/Albero/Interna',0,4);command('echo prova > Desktop/Albero/Interna/file.txt',0,5)
  command('rmdir Desktop/Albero',1,6);command('rm Desktop/Albero',1,7);command('rm -r Desktop/Albero',0,8);command('ls Desktop/Albero',1,9)
  command('rm -f Desktop/Assente',0,10);command('rm -rf /',1,11);command('rm -r /System',1,12)
  command('mkdir Desktop/DaFinder Desktop/DaFinder/Interna',0,13);command('touch Desktop/DaFinder/Interna/prova',0,14)
  g.cmd('open Desktop');g.visible('Scrivania');g.visible('DaFinder');g.type('DaFinder');g.key('delete');g.visible('Eliminare questo elemento?');g.visible('La cartella e tutto il suo contenuto');g.shot('finder-recursive-confirm');g.key('ret');time.sleep(.6);g.key('f2');g.visible('Sessione locale');command('ls Desktop/DaFinder',1,15);g.shot('terminal-deletion-results');ok('Finder confirmed deletion removes nested directories and files')
  g.cmd('open -a "About This Computer"');g.visible('Versione 10.1.7');g.visible('Build 30B8');g.shot('about-updated-version');ok('About updated to 10.1.7 / 30B8 by latest user request; layout unchanged')
 class Rec(RecoveryKey,Retail):
  recovery_text='Ripristina da una copia locale'
  def key(self,k,dt=.25,hold=None):
   if hold is None:hold=.30 if fw=='bios' else .04
   if fw=='bios' and '-' in k:
    parts=k.split('-');mods=parts[:-1];key=parts[-1]
    def edge(keys,down):self.req('input-send-event',events=[{'type':'key','data':{'down':down,'key':{'type':'qcode','data':p}}} for p in keys])
    edge(mods,True);time.sleep(.20);edge([key],True);time.sleep(hold);edge([key],False);time.sleep(.12);edge(list(reversed(mods)),False);time.sleep(dt);return
   return super().key(k,dt,hold)
 with Rec(iso,w,ev,'recovery',live=True,launcher=launcher) as g:
  g.move(60,80);g.shot('recovery-opencore');g.key('down');g.key('down');g.key('down');g.key('ret');g.visible('recovery %');g.cmd('sw_vers','tinymacOS Recovery 10.1.7 (30B8)');g.shot('recovery-version');ok('OpenCore Recovery boots, keyboard opens terminal and reports 10.1.7 / 30B8')
 r['passed']=True
finally:(q/('staging-'+fw+'-result.json')).write_text(json.dumps(r,indent=2,ensure_ascii=False)+'\n')
