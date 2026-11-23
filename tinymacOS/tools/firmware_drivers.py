#!/usr/bin/env python3
"""One verified driver set for ISO root, EFI image, install ESP and OpenDuet.
Official OpenCore artifacts and the declared PS/2 derivative stay separate.
"""
import hashlib,json,shutil,sys
from pathlib import Path

def driver_sources(root=None):
    root=Path(root) if root else Path(__file__).resolve().parents[1]
    v=root/'vendor/opencore-1.0.8'
    for name,digest in json.loads((v/'vendor-lock.json').read_text())['files'].items():
        if hashlib.sha256((v/name).read_bytes()).hexdigest()!=digest:
            raise ValueError('Official OpenCore artifact hash mismatch: '+name)
    derived=root/'vendor/tmc-ps2-keymap'
    lock=json.loads((derived/'build-lock.json').read_text())
    for name,digest in {**lock['inputs'],lock['binary']:lock['binary_sha256']}.items():
        if hashlib.sha256((derived/name).read_bytes()).hexdigest()!=digest:
            raise ValueError('Derived firmware source/binary mismatch: '+name)
    result={name:v/name for name in ['BiosVideo.efi','XhciDxe.efi','OpenUsbKbDxe.efi','UsbMouseDxe.efi']}
    result[lock['binary']]=derived/lock['binary']
    return result

def stage(destination):
    dst=Path(destination);sources=driver_sources()
    dst.mkdir(parents=True,exist_ok=True)
    for name,source in sources.items():shutil.copyfile(source,dst/name)
    # The previous official PS/2 binary remains vendored but is not also loaded.
    (dst/'Ps2KeyboardDxe.efi').unlink(missing_ok=True)

if __name__=='__main__':
    if len(sys.argv)!=2:raise SystemExit('usage: firmware_drivers.py /staging/EFI/OC/Drivers')
    stage(sys.argv[1])
