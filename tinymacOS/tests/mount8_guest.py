#!/usr/bin/env python3
"""Real live guest mount/unmount via native commands and both context menus.
Disposable FAT32 partition on NVMe, not USB certification. No action injection.
Usage: python3 tests/mount8_guest.py /external/candidate.iso
"""
from pathlib import Path
import os,sys,time,tempfile,subprocess,struct,json,hashlib
from retail_guest import Retail
from guest import KEYS
from interaction7_checks import geometry
ROOT=Path(__file__).resolve().parents[1]
os.environ.update(TMC_MEM='768',TMC_SMP='2',TMC_POINTER='tablet')
class QA(Retail):
    def type(self,text):
        for c in text:self.key(KEYS.get(c,('shift-'+c.lower() if c.isupper() else c)),dt=.10,hold=.07)
def launch(g,app,label):
    g.key('meta_l-spc');g.type(app);g.key('ret');g.visible(label);time.sleep(.6)
def command(g,s,expected):
    g.key('meta_l-k');g.cmd(s,expected,timeout=45);time.sleep(.25)
def right(g,x,y):
    g.move(x,y)
    for down in [True,False]:g.req('input-send-event',events=[{'type':'btn','data':{'button':'right','down':down}}]);time.sleep(.10)
def choose(g,text):
    x,y=g.visible(text,faces=(1,));g.click(x+10,y+4);time.sleep(.6)
def main():
    iso=Path(sys.argv[1]).resolve();w=Path(tempfile.mkdtemp(prefix='tmc-mount8-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
    fat=w/'fat.img';fat.touch();os.truncate(fat,64<<20)
    subprocess.run(['/usr/sbin/mkfs.fat','-F32',str(fat)],check=True,stdout=subprocess.DEVNULL)
    proof=w/'proof.txt';proof.write_text('MOUNT_PROOF_8\n');subprocess.run(['mcopy','-i',str(fat),str(proof),'::proof.txt'],check=True)
    disk=w/'external.raw';mbr=bytearray(512);mbr[510:]=b'\x55\xaa';mbr[450]=12;struct.pack_into('<II',mbr,454,2048,(64<<20)//512)
    with disk.open('wb') as out,fat.open('rb') as src:
        out.write(mbr);out.truncate(65<<20);offset=1<<20
        while chunk:=src.read(1<<20):
            if any(chunk):out.seek(offset);out.write(chunk)
            offset+=len(chunk)
    fat.unlink();before=hashlib.sha256(disk.read_bytes()).hexdigest()
    launcher=w/'launch.sh';launcher.write_text('#!/bin/bash\nexec '+str(ROOT/'tools/run-qemu.sh')+' -drive if=none,id=mountfixture,format=raw,file='+str(disk)+' -device nvme,drive=mountfixture,serial=MOUNT8 "$@"\n');launcher.chmod(0o755)
    r={'work':str(w),'iso_sha256':hashlib.sha256(iso.read_bytes()).hexdigest(),'transport':'NVMe disposable fixture; no USB storage claim','checks':[]}
    with QA(iso,w,ev,'mount8',live=True,launcher=launcher) as g:
        g.enroll(name='MountQA');launch(g,'Terminale','Sessione locale')
        command(g,'cat /Volumes/disk0s1/proof.txt','MOUNT_PROOF_8')
        command(g,'umount disk0s1','Volume smontato.')
        command(g,'mount','Non montato')
        command(g,'mount disk0s1','Volume montato.')
        command(g,'cat /Volumes/disk0s1/proof.txt','MOUNT_PROOF_8')
        command(g,'diskutil unmount disk0s1','Volume smontato.')
        command(g,'diskutil mount /Volumes/disk0s1','Volume montato.')
        command(g,'umount /','Volume di avvio: sempre montato.')
        g.shot('01-terminal-native-mount');r['checks'].append('mount/umount/diskutil mount/unmount are real native commands; state list, remount readback and startup refusal')
        launch(g,'Finder','Preferiti')
        x,y=g.visible('Preferiti',faces=(0,));gx,gy,_=geometry('Preferiti',0);cx=x-gx-16;cy=y-gy-14
        g.move(cx+70,cy+220)
        for _ in range(8):
            for down in [True,False]:g.req('input-send-event',events=[{'type':'btn','data':{'button':'wheel-down','down':down}}])
            time.sleep(.08)
        x,y=g.visible('disk0s1',box=(cx,cy,cx+170,cy+390),faces=(0,));right(g,x+12,y+4);g.visible('Smonta',faces=(1,));g.shot('02-finder-smonta');choose(g,'Smonta')
        x,y=g.visible('disk0s1',box=(cx,cy,cx+170,cy+390),faces=(0,));right(g,x+12,y+4);g.visible('Monta',faces=(1,));g.shot('03-finder-monta');choose(g,'Monta')
        r['checks'].append('Finder sidebar right click switches Smonta to Monta after real unmount; volume stays in inventory and can be remounted')
        launch(g,'Utility Disco','Disco 0');x,y=g.visible('s1',faces=(0,));right(g,x+10,y+4);g.visible('Smonta',faces=(1,));g.shot('04-diskutility-smonta');choose(g,'Smonta')
        x,y=g.visible('s1',faces=(0,));right(g,x+10,y+4);g.visible('Monta',faces=(1,));g.shot('05-diskutility-monta');choose(g,'Monta')
        launch(g,'Terminale','Sessione locale');command(g,'cat /Volumes/disk0s1/proof.txt','MOUNT_PROOF_8')
        r['checks'].append('Disk Utility partition sidebar right click switches real state in both directions; native Terminal reads the same file afterwards')
    after=hashlib.sha256(disk.read_bytes()).hexdigest();assert before==after,(before,after)
    listing=subprocess.check_output(['mdir','-i',str(disk)+'@@1048576','::/'],text=True);assert 'proof' in listing
    r['partition_image_sha256_before_and_after']=after;r['checks'].append('Entire disposable partitioned disk SHA256 unchanged; mtools independently reads FAT32 after guest')
    (w/'result.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2),flush=True)
if __name__=='__main__':main()
