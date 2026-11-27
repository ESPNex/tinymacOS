#!/usr/bin/env python3
"""Prevent accidental resurrection of the rejected alternate USB input path."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
retired=('drivers/usbinput.c','drivers/uhci.c','drivers/hidconfig.c',
 'include/usbhost.h','include/usb_retry.h','compat/linux/usb_input.c',
 'compat/linux/usb_keyboard.c','compat/linux/usb_generic.c',
 'compat/linux/usb_config.c','compat/linux/usb_hub.c','compat/linux/hid_parser.c')
for n in retired:assert not (r/n).exists(),'Retired native USB source restored: '+n
for directory in ('drivers','kernel'):
 for p in (r/directory).glob('*.c'):
  for line in p.read_text().splitlines():
   assert not (line.lstrip().startswith('#if') and 'TMC_USB_CANONICAL' in line),str(p)
build=(r/'build.sh').read_text();assert 'bash tools/build-usbcore.sh' in build
assert 'if [[ "${TMC_USB_CANONICAL' not in build
print('PASS USB retirement: rejected facade sources absent; no alternate USB selector in kernel/drivers/build')
