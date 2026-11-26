#!/usr/bin/env python3
"""Production binary guard: actual entry selects canonical USB/HID + worker.
Same profile found in the hash-verified published 10.2.4 ISO. Not hardware QA.
"""
from pathlib import Path
import subprocess,struct,json,hashlib,sys
out=Path(sys.argv[1]);record={'profile':'canonical-usb-hid','passed':False,'kernels':{}}
def address_bytes(b,va,n):
 off=struct.unpack_from('<Q',b,32)[0];size,count=struct.unpack_from('<HH',b,54)
 for i in range(count):
  t,fl,fo,a,pa,fs,ms,al=struct.unpack_from('<IIQQQQQQ',b,off+size*i)
  if t==1 and a<=va and va+n<=a+fs:return b[fo+va-a:fo+va-a+n]
 raise AssertionError(hex(va))
for debug,retail in [('kernel.debug','kernel.elf'),('recovery-kernel.debug','RecoveryKernel.elf')]:
 symbols={}
 for line in subprocess.check_output(['nm','-S','--defined-only',str(out/debug)],text=True).splitlines():
  a=line.split()
  if len(a)==4:symbols[a[3]]=(int(a[0],16),int(a[1],16))
 for n in ['input_init','tmc_usb_canonical_start','tmc_usb_kernel_worker','scheduler_init','tmc_usb_stack_init','tmc_usbcore_hid_parse_report','tmc_usbcore_hidinput_connect','tmc_usbcore_hid_add_device']:assert n in symbols,n
 b=(out/retail).read_bytes();at,n=symbols['input_init'];entry=address_bytes(b,at,n)
 assert len(entry)==5 and entry[0]==0xe9,'input_init must directly select canonical startup'
 target=at+5+struct.unpack_from('<i',entry,1)[0];assert target==symbols['tmc_usb_canonical_start'][0]
 sa,sn=symbols['scheduler_init'];worker=symbols['tmc_usb_kernel_worker'][0];assert b'\xbf'+struct.pack('<I',worker) in address_bytes(b,sa,sn),'scheduler must create the canonical worker'
 record['kernels'][retail]={'sha256':hashlib.sha256(b).hexdigest(),'input_init':hex(at),'entry_bytes':entry.hex(),'canonical_start':hex(target),'scheduler_references_worker':hex(worker),'whole_HID_symbols_present':True}
record['passed']=True;(out/'usb-profile.json').write_text(json.dumps(record,indent=2)+'\n')
print('PASS production USB binary profile: live and Recovery entry jumps to canonical USB/HID; scheduler creates canonical worker; full HID linked')
