#!/bin/sh
set -eu
cd "$(dirname "$0")/..";. tools/env.sh
export SOURCE_DATE_EPOCH=1735689600 LC_ALL=C TZ=UTC MTOOLS_SKIP_CHECK=1 MTOOLS_DATE_STRING=2025-01-01T00:00:00
D=$TMC_BUILD;python3 tools/media_path.py "$D" "$D/$TMC_ISO";R=$(mktemp -d "$D/disc.XXXXXXXX");trap 'rm -rf "$R"' EXIT
mkdir -p "$R/EFI/BOOT" "$R/System/Library/KernelCollections" "$R/Recovery" "$R/LICENSES";cp "$D/KernelCache" "$R/System/Library/KernelCollections/";cp "$D/BOOTX64.EFI" "$R/EFI/BOOT/";cp licenses/* "$R/LICENSES/";cp docs/QUICKSTART.txt "$R/START.TXT"
# BaseSystem is created in external scratch. Only the finished ISO is distributed.
P=${1:-};if [ -z "$P" ];then P="$R/work";mkdir "$P";python3 tools/baseimage.py "$P" "$D/RECOVERY.EFI";fi
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
mcopy -i "$V" "$D/BootConfig.bin" ::/BOOTCFG.BIN
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
mkesp "$D/install.esp" 24M LEVANTE System-disk.vol
# FAT16 ESP: force removable fallback through OpenCore for BOTH firmwares.
# Otherwise OpenDuet BDS bypasses OC, and its USB input drivers never load.
mmd -i "$D/install.esp" ::/EFI/TMC ::/EFI/OC ::/EFI/OC/Drivers ::/LICENSES
mcopy -i "$D/install.esp" "$D/BOOTX64.EFI" ::/EFI/TMC/BOOTX64.EFI
mcopy -i "$D/install.esp" vendor/opencore-1.0.8/OpenCore.efi ::/EFI/OC/OpenCore.efi
mcopy -o -i "$D/install.esp" vendor/opencore-1.0.8/BOOTx64.efi ::/EFI/BOOT/BOOTX64.EFI
for driver in BiosVideo XhciDxe OpenUsbKbDxe UsbMouseDxe;do
 mcopy -i "$D/install.esp" "vendor/opencore-1.0.8/$driver.efi" "::/EFI/OC/Drivers/$driver.efi"
done
mcopy -i "$D/install.esp" boot/opencore.plist ::/EFI/OC/config.plist
mcopy -i "$D/install.esp" vendor/opencore-1.0.8/LICENSE.txt ::/LICENSES/OpenCore.txt
mkesp "$R/EFI.IMG" 64M LEVANTECD System-live.vol;mcopy -i "$R/EFI.IMG" "$D/install.esp" ::/Install.esp;mmd -i "$R/EFI.IMG" ::/Recovery;mcopy -i "$R/EFI.IMG" "$F" ::/Recovery/BaseSystem.dmg;mcopy -i "$R/EFI.IMG" "$P/Volume.hdr" ::/Recovery/Volume.hdr;python3 -c 'import hashlib,pathlib,sys;e,r,out=map(pathlib.Path,sys.argv[1:]);out.write_bytes(hashlib.sha256(e.read_bytes()).digest()+hashlib.sha256(r.read_bytes()).digest())' "$D/install.esp" "$V" "$D/Payload.sha";mcopy -i "$R/EFI.IMG" "$D/Payload.sha" ::/Recovery/Payload.sha
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
xorriso -as mkisofs -quiet -r -J -V TINYMACOS_2_4_4 --modification-date=2025010100000000 --set_all_file_dates 2025010100000000 -e EFI.IMG -no-emul-boot -o "$D/$TMC_ISO" "$R"
