#!/bin/sh
set -eu
cd "$(dirname "$0")/..";. tools/env.sh
export SOURCE_DATE_EPOCH=1735689600 LC_ALL=C TZ=UTC MTOOLS_SKIP_CHECK=1 MTOOLS_DATE_STRING=2025-01-01T00:00:00
D=$TMC_BUILD;python3 tools/media_path.py "$D" "$D/$TMC_ISO";R=$(mktemp -d "$D/disc.XXXXXXXX");trap 'rm -rf "$R"' EXIT
mkdir -p "$R/EFI/TMC" "$R/EFI/OC/Drivers" "$R/EFI/OC/Tools" "$R/EFI/BOOT" "$R/System/Library/KernelCollections" "$R/Recovery" "$R/LICENSES";cp "$D/KernelCache" "$R/System/Library/KernelCollections/";cp "$D/BOOTX64.EFI" "$R/EFI/TMC/BOOTX64.EFI";cp vendor/opencore-1.0.8/BOOTx64.efi "$R/EFI/BOOT/BOOTX64.EFI";cp vendor/opencore-1.0.8/OpenCore.efi "$R/EFI/OC/";cp boot/opencore.plist "$R/EFI/OC/config.plist";python3 tools/firmware_drivers.py "$R/EFI/OC/Drivers";cp licenses/* "$R/LICENSES/";cp docs/QUICKSTART.txt "$R/START.TXT"
cp "$D/TmcVideo.EFI" "$R/EFI/OC/Tools/TmcVideo.efi"
cp "$D/TmcRestore.EFI" "$R/EFI/OC/Tools/TmcRestore.efi"
# BaseSystem is created in external scratch. Only the finished ISO is distributed.
# Firmware still launches these files normally. Only OpenCore's OS scan must
# hide its own bootstrap/manager, or a registered Boot#### can recurse.
printf 'Disabled\n' > "$D/opencore.contentVisibility"
cp "$D/opencore.contentVisibility" "$R/EFI/BOOT/.contentVisibility"
cp "$D/opencore.contentVisibility" "$R/EFI/OC/.contentVisibility"
P=${1:-};if [ -z "$P" ];then P="$R/work";mkdir "$P";python3 tools/baseimage.py "$P" "$D/RecoveryKernel.elf" "$D/System-recovery.vol";fi
python3 tools/media_path.py "$P" "$P/RecoveryHD.fat"
python3 - "$D/BootConfig.bin" <<'PYCFG'
import sys,struct,hashlib
from pathlib import Path
p=struct.pack('<8sII',b'TMCBOOT1',1,0);Path(sys.argv[1]).write_bytes(p+hashlib.sha256(p).digest()+bytes(464))
PYCFG
F="$P/BaseSystem.dmg";V="$P/RecoveryHD.fat";: >"$V";truncate -s 10M "$V"  # == NREC
mkfs.fat --invariant -F16 -s1 -n RECOVERYHD "$V" >/dev/null
mmd -i "$V" ::/EFI ::/EFI/BOOT ::/Recovery
mcopy -i "$V" "$D/RECBOOT.EFI" ::/EFI/BOOT/BOOTX64.EFI
# Only the installed Recovery config requests a dedicated OpenCore default.
# The optical/live root retains flags=0. Microsoft loader files are untouched.
python3 - "$D/BootConfig.bin" "$D/DiskBootConfig.bin" <<'PYBOOT'
import hashlib,struct,sys
from pathlib import Path
p=bytearray(Path(sys.argv[1]).read_bytes());struct.pack_into('<I',p,12,1<<28);p[16:48]=hashlib.sha256(p[:16]).digest();Path(sys.argv[2]).write_bytes(p)
PYBOOT
mcopy -i "$V" "$D/DiskBootConfig.bin" ::/BOOTCFG.BIN
mcopy -i "$V" "$F" ::/Recovery/BaseSystem.dmg
cp "$F" "$R/Recovery/BaseSystem.dmg"
python3 - "$V" "$F" "$P/Volume.hdr" "$D" <<'PYF'
from pathlib import Path
import sys,hashlib
v,d,h,build=map(Path,sys.argv[1:]);raw=v.read_bytes();image=d.read_bytes();i=raw.index(image)
assert 0<i<len(raw) and i%512==0 and raw.count(image)==1 and not any(raw[i+len(image):])
digest=hashlib.sha256(image).digest()
assert all(digest in (build/name).read_bytes() for name in ("BOOTX64.EFI","RECBOOT.EFI")), "Stale EFI loader: rebuild with build.sh"
h.write_bytes(raw[:i])
PYF
mkesp(){ : > "$1";truncate -s "$2" "$1";mkfs.fat --invariant -R32 -F16 -n "$3" "$1" >/dev/null;mmd -i "$1" ::/EFI ::/EFI/BOOT ::/System ::/System/Library ::/System/Library/CoreServices ::/System/Library/KernelCollections;mcopy -i "$1" "$D/KernelCache" ::/System/Library/KernelCollections/KernelCache;mcopy -i "$1" "$D/$4" ::/System/Library/CoreServices/System.vol;mcopy -i "$1" "$D/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI;mcopy -i "$1" "$D/BootConfig.bin" ::/BOOTCFG.BIN; }
mkesp "$D/install.esp" 24M TRAMONTANA System-disk.vol
# FAT16 ESP: force removable fallback through OpenCore for BOTH firmwares.
# Otherwise OpenDuet BDS bypasses OC, and its USB input drivers never load.
# Precreate empty OC legacy-NVRAM directory before sealing: LoadNvram opens it
# with CREATE even with an empty schema, which would otherwise alter the seal.
mmd -i "$D/install.esp" ::/EFI/TMC ::/EFI/OC ::/EFI/OC/Drivers ::/EFI/OC/Tools ::/LICENSES ::/NVRAM
mcopy -i "$D/install.esp" "$D/BOOTX64.EFI" ::/EFI/TMC/BOOTX64.EFI
mcopy -i "$D/install.esp" vendor/opencore-1.0.8/OpenCore.efi ::/EFI/OC/OpenCore.efi
mcopy -o -i "$D/install.esp" vendor/opencore-1.0.8/BOOTx64.efi ::/EFI/BOOT/BOOTX64.EFI
for driver in BiosVideo XhciDxe OpenUsbKbDxe TmcPs2KeyboardDxe UsbMouseDxe NvmExpressDxe;do
 mcopy -i "$D/install.esp" "$R/EFI/OC/Drivers/$driver.efi" "::/EFI/OC/Drivers/$driver.efi"
done
python3 - "$D/installed-opencore.plist" <<'PYOC'
import plistlib,sys
from pathlib import Path
p=plistlib.loads(Path('boot/opencore.plist').read_bytes());p['NVRAM']['Add']={}
Path(sys.argv[1]).write_bytes(plistlib.dumps(p,sort_keys=False))
PYOC
mcopy -i "$D/install.esp" "$D/TmcVideo.EFI" ::/EFI/OC/Tools/TmcVideo.efi
mcopy -i "$D/install.esp" "$D/TmcRestore.EFI" ::/EFI/OC/Tools/TmcRestore.efi
mcopy -i "$D/install.esp" "$D/installed-opencore.plist" ::/EFI/OC/config.plist
mcopy -i "$D/install.esp" "$D/opencore.contentVisibility" ::/EFI/BOOT/.contentVisibility
mcopy -i "$D/install.esp" "$D/opencore.contentVisibility" ::/EFI/OC/.contentVisibility
mcopy -i "$D/install.esp" vendor/opencore-1.0.8/LICENSE.txt ::/LICENSES/OpenCore.txt
# Installed media must retain Linux GPL and all bundled component notices,
# not only the firmware notices visible on the optical root.
for notice in licenses/*;do mcopy -i "$D/install.esp" "$notice" "::/LICENSES/$(basename "$notice")";done
# Genuine native-4Kn FAT media: 4KiB BPB sectors, identical file contents.
rm -f "$D/install-4k.esp" "$D/recovery-4k.img"
python3 tools/fat-native-4k.py "$D/install.esp" "$D/install-4k.esp"
python3 tools/fat-native-4k.py "$V" "$D/recovery-4k.img"
python3 - "$D" <<'PY4KHASH'
from pathlib import Path
import hashlib,sys
p=Path(sys.argv[1]);(p/'Payload4K.sha').write_bytes(b''.join(hashlib.sha256((p/n).read_bytes()).digest() for n in ['install-4k.esp','recovery-4k.img']))
PY4KHASH
mkesp "$R/EFI.IMG" 128M TRAMONTANA System-live.vol
mmd -i "$R/EFI.IMG" ::/EFI/TMC ::/EFI/OC ::/EFI/OC/Drivers ::/EFI/OC/Tools ::/NVRAM ::/LICENSES
mcopy -i "$R/EFI.IMG" vendor/opencore-1.0.8/LICENSE.txt ::/LICENSES/OpenCore.txt
for notice in licenses/*;do mcopy -i "$R/EFI.IMG" "$notice" "::/LICENSES/$(basename "$notice")";done
mcopy -i "$R/EFI.IMG" "$D/BOOTX64.EFI" ::/EFI/TMC/BOOTX64.EFI
mcopy -o -i "$R/EFI.IMG" vendor/opencore-1.0.8/BOOTx64.efi ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "$R/EFI.IMG" vendor/opencore-1.0.8/OpenCore.efi ::/EFI/OC/OpenCore.efi
mcopy -i "$R/EFI.IMG" "$D/TmcVideo.EFI" ::/EFI/OC/Tools/TmcVideo.efi
mcopy -i "$R/EFI.IMG" "$D/TmcRestore.EFI" ::/EFI/OC/Tools/TmcRestore.efi
mcopy -i "$R/EFI.IMG" boot/opencore.plist ::/EFI/OC/config.plist
mcopy -i "$R/EFI.IMG" "$D/opencore.contentVisibility" ::/EFI/BOOT/.contentVisibility
mcopy -i "$R/EFI.IMG" "$D/opencore.contentVisibility" ::/EFI/OC/.contentVisibility
for driver in BiosVideo XhciDxe OpenUsbKbDxe TmcPs2KeyboardDxe UsbMouseDxe NvmExpressDxe;do mcopy -i "$R/EFI.IMG" "$R/EFI/OC/Drivers/$driver.efi" "::/EFI/OC/Drivers/$driver.efi";done
mcopy -i "$R/EFI.IMG" "$D/install.esp" ::/Install.esp;mmd -i "$R/EFI.IMG" ::/Recovery;mcopy -i "$R/EFI.IMG" "$F" ::/Recovery/BaseSystem.dmg;mcopy -i "$R/EFI.IMG" "$P/Volume.hdr" ::/Recovery/Volume.hdr;python3 -c 'import hashlib,pathlib,sys;e,r,out=map(pathlib.Path,sys.argv[1:]);out.write_bytes(hashlib.sha256(e.read_bytes()).digest()+hashlib.sha256(r.read_bytes()).digest())' "$D/install.esp" "$V" "$D/Payload.sha";mcopy -i "$R/EFI.IMG" "$D/Payload.sha" ::/Recovery/Payload.sha
for asset in install-4k.esp recovery-4k.img Payload4K.sha;do
 mcopy -i "$R/EFI.IMG" "$D/$asset" "::/$asset"
 cp "$D/$asset" "$R/$asset"
done
# USB sticks written in ISO mode (the Rufus default) boot the ISO9660 root,
# not the FAT El Torito image: carry the same payload files there too, or the
# loader finds no System.vol / Install.esp and can only fall back to Recupero.
cp "$D/install.esp" "$R/Install.esp"
cp "$D/BootConfig.bin" "$R/BOOTCFG.BIN"
mkdir -p "$R/System/Library/CoreServices"
cp "$D/System-live.vol" "$R/System/Library/CoreServices/System.vol"
cp "$P/Volume.hdr" "$R/Recovery/Volume.hdr"
cp "$D/Payload.sha" "$R/Recovery/Payload.sha"
if [ "$P" = "$R/work" ];then rm -rf "$P";fi
find "$R" -exec touch -h -d @1735689600 {} +
xorriso -as mkisofs -quiet -r -J -V TINYMACOS_10_2_5 --modification-date=2025010100000000 --set_all_file_dates 2025010100000000 -e EFI.IMG -no-emul-boot -o "$D/$TMC_ISO" "$R"
