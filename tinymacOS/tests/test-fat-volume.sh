#!/bin/bash
# Standalone actual writable driver gate. NOT namespace/syscall/guest acceptance.
set -euo pipefail
cd "$(dirname "$0")/.."
export PATH=/usr/sbin:$PATH
out=$(mktemp -d /var/tmp/tmc-fat-volume-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(--sanitize)
 python3 tools/build-fatfs.py "$out/$mode" "${flags[@]}"
 cc=(-O1 -g -fno-builtin -fno-pie -no-pie -ffunction-sections -fdata-sections -Iinclude)
 [[ $mode != sanitized ]] || cc+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc "${cc[@]}" tests/fat_volume_host.c "$out/$mode/k-native-fat.o" kernel/string.c -Wl,--gc-sections -o "$out/$mode-test"
 for fs in fat32 exfat format;do
  image="$out/$mode-$fs.img";truncate -s 64M "$image"
  if [[ $fs == fat32 ]];then mkfs.fat -F32 "$image" >/dev/null
  elif [[ $fs == exfat ]];then mkfs.exfat "$image" >/dev/null;fi
  "$out/$mode-test" "$fs" "$image"
  if [[ $fs == fat32 ]];then fsck.fat -n "$image";else fsck.exfat -n "$image";fi
 done
done
echo 'PASS actual standalone FAT32/exFAT read/write and native exFAT partition formatter; independent fsck; normal + ASan/UBSan. Not integrated FS2 namespace/guest acceptance.'
