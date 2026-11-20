#!/usr/bin/env python3
"""Boot an ISO-mode-equivalent GPT/FAT32 USB in QEMU, with NO CD attached.
This is not Rufus running on Windows and not a physical hardware test.
Requires the disposable image in /var/tmp/tmc-usb-iso-mode-test/usb-gpt.raw.
"""
import json,os,shutil,socket,subprocess,time
from pathlib import Path
from PIL import Image
w=Path('/var/tmp/tmc-usb-iso-mode-test')
ev=Path(__file__).resolve().parent
tr=Path('/var/tmp/tmc/tools/root')
shutil.copyfile(tr/'usr/share/OVMF/OVMF_VARS_4M.fd',w/'vars.fd')
env={**os.environ,'LD_LIBRARY_PATH':str(tr/'usr/lib/x86_64-linux-gnu')}
sock=w/'qmp.sock'
cmd=[str(tr/'usr/bin/qemu-system-x86_64'),'-L',str(tr/'usr/share/qemu'),'-L',str(tr/'usr/share/seabios'),
 '-machine','q35,i8042=off','-accel','tcg','-cpu','max','-smp','2','-m','1024','-vga','std',
 '-drive',f'if=pflash,format=raw,readonly=on,file={tr}/usr/share/OVMF/OVMF_CODE_4M.fd',
 '-drive',f'if=pflash,format=raw,file={w}/vars.fd',
 '-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0,port=1',
 '-device','usb-tablet,bus=xhci.0,port=2',
 '-drive',f'if=none,id=usb,format=raw,readonly=on,file={w}/usb-gpt.raw',
 '-device','usb-storage,drive=usb,bus=xhci.0,port=3,bootindex=1',
 '-object','rng-random,filename=/dev/urandom,id=rng','-device','virtio-rng-pci,rng=rng',
 '-display','none','-serial',f'file:{w}/serial.log','-qmp',f'unix:{sock},server=on,wait=off']
p=subprocess.Popen(cmd,env=env,stdout=(w/'qemu.log').open('wb'),stderr=subprocess.STDOUT)
try:
 for _ in range(100):
  if sock.exists():break
  if p.poll() is not None:raise RuntimeError((w/'qemu.log').read_text())
  time.sleep(.1)
 s=socket.socket(socket.AF_UNIX);s.settimeout(15);s.connect(str(sock));f=s.makefile('rwb',buffering=0);f.readline()
 def q(cmd,**args):
  f.write(json.dumps({'execute':cmd,'arguments':args}).encode()+b'\n')
  while True:
   r=json.loads(f.readline())
   if 'error' in r:raise RuntimeError(r)
   if 'return' in r:return r['return']
 q('qmp_capabilities');time.sleep(22)
 q('screendump',filename=str(w/'usb-boot.ppm'))
 Image.open(w/'usb-boot.ppm').save(ev/'usb-iso-mode-boot.png')
 report={'vm_status':q('query-status'),'boot_medium':'GPT/FAT32 via emulated USB mass storage, ISO root extracted and copied','cd_attached':False,'physical_hardware_test':False,'rufus_executed':False,'result':'Screenshot requires visual inspection'}
 (ev/'usb-boot-result.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
 q('quit');p.wait(timeout=10)
finally:
 if p.poll() is None:p.terminate();p.wait(timeout=10)
