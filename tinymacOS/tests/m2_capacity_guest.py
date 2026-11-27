"""M2: real >2TiB/4Kn GPT-gap install, same-media UI capacities and cold boot.
Synthetic foreign partitions (not Windows certification), never physical media.
"""
from pathlib import Path
import hashlib,json,os,struct,tempfile,time,uuid,zlib
from retail_guest import Retail
from boot_recovery import RecoveryKey
from m2_sparse_check import verify_foreign
iso=Path(os.environ['TMC_STAGE_ISO']);qa=Path(os.environ['TMC_QA_DIR']);qa.mkdir(parents=True,exist_ok=True)
w=Path(tempfile.mkdtemp(prefix='tmc-m2-capacity-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();disk=w/'target/system.raw';disk.parent.mkdir()
logical=int(os.environ.get('TMC_M2_LOGICAL','4096'));scale=logical//512
bus=os.environ.get('TMC_M2_STORAGE','nvme');assert bus in ('nvme','sata') and (bus=='nvme' or logical==512)
total=3*(1<<31);start=2097152;span=5*(1<<30);end=start+span
ranges=[(2048,start-1),(end,total-2049)]
os.environ.update(TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',TMC_STORAGE=bus,TMC_NVME_LBA_BYTES=str(logical))
def size(n):
    units=[' B',' kB',' MB',' GB',' TB'];i=0;div=1
    while i<4 and n//div>=1000:i+=1;div*=1000
    return str(n) if not i else str(n//div)+'.'+str(n%div//(div//10))+units[i]
parts=bytearray(16384);ident=uuid.UUID(int=75321).bytes_le;physical=total//scale
for i,(a,b) in enumerate(ranges):struct.pack_into('<16s16sQQQ72s',parts,i*128,uuid.UUID('ebd0a0a2-b9e5-4433-87c0-68b6b72699c7').bytes_le,uuid.UUID(int=i+11).bytes_le,a//scale,(b+1)//scale-1,0,('FOREIGN_'+str(i)).encode('utf-16le'))
def hdr(at,other,entries):
    h=bytearray(512);table=(32+scale-1)//scale
    struct.pack_into('<8sIIIIQQQQ16sQIII',h,0,b'EFI PART',65536,92,0,0,at,other,2+table,physical-2-table,ident,entries,128,128,zlib.crc32(parts));struct.pack_into('<I',h,16,zlib.crc32(h[:92]));return h
with disk.open('wb') as f:
    f.truncate(total*512);m=bytearray(512);m[510:]=b'\x55\xaa';m[450]=0xee;struct.pack_into('<II',m,454,1,min(physical-1,0xffffffff))
    table=(32+scale-1)//scale
    for at,b in [(0,m),(scale,hdr(1,physical-1,2)),(2*scale,parts),((physical-1-table)*scale,parts),((physical-1)*scale,hdr(physical-1,1,physical-1-table))]:f.seek(at*512);f.write(b)
    for i,(a,b) in enumerate(ranges):
        for pos in (a,b-7):f.seek(pos*512);f.write(('FOREIGN_%d_%d'%(i,pos)).encode().ljust(4096,b'R'))
def fingerprints():
    with disk.open('rb') as f:
        result=[]
        for a,b in ranges:
            for pos in (a,b-7):f.seek(pos*512);result.append(hashlib.sha256(f.read(4096)).hexdigest())
        f.seek(2*logical);result.append(hashlib.sha256(f.read(256)).hexdigest())
        return result
before=fingerprints();capacity=size(total*512);allocated=size(span*512)
r={'passed':False,'release_ready':False,'physical_hardware_tested':False,'synthetic_foreign_partitions':True,'native_logical_bytes':logical,'storage_bus':bus,'disk_sectors':total,'selected_gap_sectors':span,'work':str(w),'checks':[],'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()}
class QA(Retail):
    def type(self,text):
        from guest import KEYS
        for c in text:self.key(KEYS.get(c,('shift-'+c.lower() if c.isupper() else c)),dt=.055,hold=.025)
class Installed(QA):initial='CapacityQA'
class Recovery(RecoveryKey,QA):recovery_text='Ripristina da una copia locale'
def terminal(g):g.key('f2');g.move(60,700);g.visible('Sessione locale')
def utility(g,item):
    x,y=g.visible('Utility',box=(0,0,1000,34));g.click(x+12,y+8);x,y=g.visible(item,box=(0,34,1200,330));g.click(x+15,y+8);g.move(60,700)
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
try:
    with QA(iso,w,ev,'m2-live',disk='target',live=False) as g:
        g.enroll(name='CapacityQA');terminal(g);g.cmd('echo M2_NATIVE_IMPORT > Documents/m2-proof.txt');g.cmd('diskutil list');g.visible(capacity);g.visible(allocated);g.visible('FOREIGN_1')
        g.cmd('open -a "Disk Utility"');g.visible('Disco interno');g.visible(capacity);g.visible('Non allocato · '+allocated);g.shot('m2-live-utility-gap')
        g.key('ctrl-w');g.key('f11');g.visible('Installa tinymacOS Tramontana');g.key('ret');g.visible('Seleziona la destinazione');g.visible(allocated+' non allocati');g.key('down');g.key('ret');g.visible('Conferma l’installazione');assert fingerprints()==before;g.shot('m2-installer-genuine-gap')
        g.key('spc');g.key('ret');g.visible('Il disco è pronto',600);g.key('ret');g.p.wait(timeout=20)
    assert fingerprints()==before
    with disk.open('rb') as f:
        f.seek(2*logical);p=f.read(16384);a,b=struct.unpack_from('<QQ',p,2*128+32);assert a*scale==start and (b+1)*scale==end
        f.seek((start+9)*512);h=f.read(512);assert h[:8]==b'TMCVOL23' and struct.unpack_from('<I',h,8)[0]==3
        stored=struct.unpack_from('<I',h,64)[0];hc=bytearray(h);struct.pack_into('<I',hc,64,0);assert zlib.crc32(hc)==stored
        span64,rec,source,duet=struct.unpack_from('<QQQQ',h,68);assert span64==span and rec==span-73728 and source==rec+20480 and duet==source+49153
        f.seek((start+49152)*512);sb=f.read(128);assert sb[:8]==b'TMCBFS2\0';assert struct.unpack_from('<Q',sb,16)[0]==(rec-49152)//32
        r['actual_data_sectors']=rec-49152
    ok('Real GUI 3TiB '+str(logical)+'B install: v3 owns entire 2.5TiB genuine gap, exact FS2 span, preserved foreign GPT entries/boundaries, no 32-bit wrap')
    with Installed('/not-attached',w,ev,'m2-cold-installed',disk='target',boot_disk=True) as g:
        g.unlock();terminal(g);g.cmd('cat Documents/m2-proof.txt','M2_NATIVE_IMPORT');g.cmd('diskutil list');g.visible(capacity);g.visible(allocated);g.visible('FOREIGN_1')
        g.cmd('diskutil apfs list');g.visible(size(r['actual_data_sectors']*512),timeout=180)
        g.cmd('open -a "Disk Utility"');g.visible('Disco interno');g.visible(capacity);g.shot('m2-cold-utility-disk');g.key('down');g.key('down');g.visible('Dati tinymacOS');g.visible('Utilizzo');g.shot('m2-cold-utility-data');terminal(g);g.cmd('off');g.p.wait(timeout=20)
    ok('ISO-detached cold native boot: imported account/file usable; Terminal physical/container/data sizes and Utility physical disk agree with independent GPT/FS2 geometry')
    with Recovery('/not-attached',w,ev,'m2-cold-recovery-three-views',disk='target',boot_disk=True) as g:
        utility(g,'Terminale');g.visible('Sessione locale');g.cmd('diskutil list');g.visible(capacity);g.visible(allocated);g.shot('m2-recovery-terminal')
        utility(g,'Utility Disco');g.visible('Disco interno');g.visible(capacity);g.shot('m2-recovery-utility');g.key('ctrl-w')
        # Reinstaller is the same application launched from the Recovery card.
        g.key('ctrl-w');g.click(550,355);g.click(880,557);g.visible('Per reinstallare il sistema');g.key('ret');g.visible('Seleziona la destinazione');g.visible(capacity);g.shot('m2-recovery-installer');g.key('esc');g.key('esc')
    assert fingerprints()==before;ok('Same installed disk after cold Recovery boot: Installer, Utility Disco and Terminal all show physical capacity '+capacity+'; container '+allocated+' remains distinct and foreign metadata intact')
    r['foreign_content']=verify_foreign(disk,total,start,span)
    r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:
    (qa/('m2-capacity-'+str(logical)+'-result.json')).write_text(json.dumps(r,indent=2)+'\n')
    for p in w.glob('*.ppm'):p.unlink()
