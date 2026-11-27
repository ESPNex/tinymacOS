#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export PATH=/usr/sbin:$PATH
out=$(mktemp -d /var/tmp/tmc-foreignfs-XXXXXXXX);trap 'rm -rf "$out"' EXIT
python3 - "$out" <<'PY'
import sys
from pathlib import Path
p=Path(sys.argv[1]);(p/'proof.bin').write_bytes(bytes((i*17+23)&255 for i in range(65537)));(p/'mini.txt').write_text('CIAO')
PY
for mode in normal sanitized;do
 flags=();sanitize=();[[ $mode != sanitized ]] || { flags=(-fsanitize=address,undefined -fno-omit-frame-pointer);sanitize=(--sanitize); }
 python3 tools/build-ntfs.py "$out/$mode" "${sanitize[@]}"
 python3 tools/build-fatfs.py "$out/$mode" "${sanitize[@]}"
 truncate -s 64M "$out/$mode-fat32.img";mkfs.fat -F32 "$out/$mode-fat32.img" >/dev/null
 mcopy -i "$out/$mode-fat32.img" "$out/proof.bin" ::proof.bin;mcopy -i "$out/$mode-fat32.img" "$out/mini.txt" ::mini.txt
 truncate -s 64M "$out/$mode-ntfs.img";mkntfs -F -Q "$out/$mode-ntfs.img" >"$out/mkntfs.log" 2>&1
 ntfscp "$out/$mode-ntfs.img" "$out/proof.bin" /proof.bin;ntfscp "$out/$mode-ntfs.img" "$out/mini.txt" /mini.txt
 gcc -O1 -g -fno-builtin -no-pie -Iinclude "${flags[@]}" tests/foreignfs_host.c storage/foreignfs.c storage/exfat_copy.c kernel/string.c -o "$out/$mode-generator"
 "$out/$mode-generator" export "$out/$mode-exfat.img";fsck.exfat -n "$out/$mode-exfat.img";"$out/$mode-generator" export-read "$out/$mode-exfat.img"
 truncate -s 64M "$out/$mode-current.img";truncate -s 32M "$out/$mode-native.img";truncate -s 96M "$out/$mode-target.raw"
 gcc -O1 -g -fno-builtin -no-pie -DTMC_NTFS_HOST -ffunction-sections -fdata-sections -Iinclude -Iarch "${flags[@]}" tests/volume_export_host.c fs/volumes.c storage/foreignfs.c storage/exfat_copy.c storage/tmcbfs2.c storage/partition.c storage/maintenance.c storage/diskedit.c storage/ntfs_resize.c crypto/sha256.c kernel/string.c "$out/$mode/k-native-fat.o" "$out/$mode/k-native-ntfs.o" -Wl,--gc-sections -o "$out/$mode-adapter"
 "$out/$mode-adapter" "$out/$mode-current.img" "$out/$mode-fat32.img" "$out/$mode-ntfs.img" "$out/$mode-exfat.img" "$out/$mode-native.img" "$out/$mode-target.raw" "$out/$mode-exported.img"
 fsck.exfat -n "$out/$mode-exported.img";fsck.fat -n "$out/$mode-fat32.img";ntfsresize -i -f "$out/$mode-ntfs.img" >/dev/null
 echo "PASS $mode actual RW namespace and NEW REAL partition export, independent fsck"
done
