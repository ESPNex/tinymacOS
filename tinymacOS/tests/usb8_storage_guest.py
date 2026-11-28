#!/usr/bin/env python3
"""USB BOT block/filesystem integration in a disposable QEMU guest.
Real keyboard UI commands, real usb-storage device; NOT physical-board proof.
"""
from pathlib import Path
import os,sys,time,tempfile,subprocess,struct,json,hashlib,shlex
from mount8_guest import QA,launch,command
from interaction7_checks import cursor
ROOT=Path(__file__).resolve().parents[1]
def main():
    iso=Path(sys.argv[1]).resolve();profile=sys.argv[2] if len(sys.argv)>2 else 'xhci'
    assert profile in ('xhci','ehci')
    w=Path(tempfile.mkdtemp(prefix='tmc-usb8-'+profile+'-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
    fat=w/'fat.img';fat.touch();os.truncate(fat,64<<20)
    subprocess.run(['/usr/sbin/mkfs.fat','-F32',str(fat)],check=True,stdout=subprocess.DEVNULL)
    proof=w/'proof.txt';proof.write_text('USB8_CANONICAL_PROOF\n');subprocess.run(['mcopy','-i',str(fat),str(proof),'::proof.txt'],check=True)
    disk=w/'external.raw';mbr=bytearray(512);mbr[510:]=b'\x55\xaa';mbr[450]=12;struct.pack_into('<II',mbr,454,2048,(64<<20)//512)
    with disk.open('wb') as out,fat.open('rb') as src:
        out.write(mbr);out.truncate(65<<20);offset=1<<20
        while chunk:=src.read(1<<20):
            if any(chunk):out.seek(offset);out.write(chunk)
            offset+=len(chunk)
    fat.unlink()
    launcher=w/'launch.sh';bus=profile+'.0';base=ROOT/'tools'/('run-qemu.sh' if profile=='xhci' else 'run-input-qa.sh')
    launcher.write_text('#!/bin/bash\nexec '+shlex.quote(str(base))+' -drive if=none,id=usb8fixture,format=raw,file='+shlex.quote(str(disk))+' -device usb-storage,id=usb8disk,drive=usb8fixture,bus='+bus+',port=3,serial=USB8CANONICAL "$@"\n');launcher.chmod(0o755)
    os.environ.update(TMC_POINTER='tablet' if profile=='xhci' else 'mouse',TMC_TEST_INPUT='ehci-kbd',TMC_TEST_FIRMWARE='uefi',TMC_BUILD=str(iso.parent),TOOLROOT=os.environ.get('TOOLROOT',''),OCR_ROOT=os.environ.get('OCR_ROOT',''))
    r={'passed':False,'work':str(w),'iso_sha256':hashlib.sha256(iso.read_bytes()).hexdigest(),'transport':profile+' original HCD + canonical usbcore + native BOT/SCSI, QEMU usb-storage','physical_hardware_tested':False,'checks':[]}
    try:
        with QA(iso,w,ev,'usb8',live=True,launcher=launcher) as g:
            g.enroll(name='USBQA');launch(g,'Terminale','Sessione locale')
            command(g,'cat /Volumes/disk0s1/proof.txt','USB8_CANONICAL_PROOF')
            r['checks'].append('USB disk discovered before initial filesystem inventory; FAT32 read through real BOT and common block layer')
            cursor(g,1100,300,'Normal.cur','usb8-input-before-copy')
            command(g,'cp /Volumes/disk0s1/proof.txt /Volumes/disk0s1/copy.txt','Sessione locale')
            command(g,'cat /Volumes/disk0s1/copy.txt','USB8_CANONICAL_PROOF')
            command(g,'umount disk0s1','Volume smontato.')
            command(g,'mount disk0s1','Volume montato.')
            command(g,'cat /Volumes/disk0s1/copy.txt','USB8_CANONICAL_PROOF')
            command(g,'umount disk0s1','Volume smontato.')
            r['checks'].append('USB FAT32 write/copy, actual flush/unmount/remount and readback with USB-only keyboard and rendered cursor')
            g.req('device_del',id='usb8disk');time.sleep(3)
            command(g,'echo USB_INPUT_AFTER_DETACH','USB_INPUT_AFTER_DETACH')
            # Legacy -drive backends are automatically removed by QEMU with
            # the USB device. Recreate an explicit block graph for reattach.
            g.req('blockdev-add',**{'driver':'file','node-name':'usb8file2','filename':str(disk)})
            g.req('blockdev-add',**{'driver':'raw','node-name':'usb8raw2','file':'usb8file2'})
            g.req('device_add',driver='usb-storage',id='usb8disk2',drive='usb8raw2',bus=bus,port='3',serial='USB8CANONICAL2');time.sleep(4)
            command(g,'cat /Volumes/disk1s1/copy.txt','USB8_CANONICAL_PROOF')
            command(g,'mount','/Volumes/disk1s1')
            cursor(g,1120,320,'Normal.cur','usb8-input-after-hotplug')
            g.shot('usb8-storage-hotplug-input')
            r['checks'].append('USB hot unplug/reattach on same port uses new stable slot disk1, not stale disk0; filesystem inventory refreshed; keyboard and pointer still work')
            command(g,'umount disk1s1','Volume smontato.')
        result=subprocess.check_output(['mtype','-i',str(disk)+'@@1048576','::copy.txt'],text=True)
        assert result=='USB8_CANONICAL_PROOF\n',repr(result)
        r['checks'].append('Independent host mtools reads exact new file from USB backing medium after guest exit')
        r['passed']=True
    finally:
        (w/'result.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2),flush=True)
if __name__=='__main__':main()
