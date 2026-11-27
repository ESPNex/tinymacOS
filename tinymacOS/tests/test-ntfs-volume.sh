#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export PATH=/usr/sbin:$PATH
out=$(mktemp -d /var/tmp/tmc-ntfs-volume-XXXXXX);trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 python3 tools/build-ntfs.py "$out/$mode" ${flags:+--sanitize}
 # The non-sanitized shipping engine is exercised separately above.
 truncate -s 64M "$out/$mode.img";mkntfs -F -Q "$out/$mode.img" >"$out/mkntfs.log" 2>&1
 gcc -O1 -g -fno-builtin -no-pie -ffunction-sections -fdata-sections -DTMC_NTFS_HOST -Iinclude "${flags[@]}" tests/ntfs_volume_host.c storage/ntfs_resize.c crypto/sha256.c kernel/string.c "$out/$mode/k-native-ntfs.o" -Wl,--gc-sections -o "$out/$mode-host"
 "$out/$mode-host" ntfs "$out/$mode.img"
 ntfscat "$out/$mode.img" /occupied.txt | grep -qx PRESERVE
 ntfscat "$out/$mode.img" /Documenti/rinominato.txt | grep -qx tinytail
 ntfsresize -i -f "$out/$mode.img" >"$out/independent.log" 2>&1
 echo "PASS $mode native NTFS RW + independent ntfscat payload + ntfsresize consistency"
done
