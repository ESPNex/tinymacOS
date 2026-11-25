#!/bin/bash
# Uses only generated temporary files, never /dev or a user disk image.
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/env.sh
out=$(mktemp -d /var/tmp/tmc-diskedit-test-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
truncate -s 64M "$out/input.fat"
mkfs.fat --invariant -F32 -s1 "$out/input.fat" > /dev/null
printf 'tinymacOS FAT32 resize: file bytes must survive both operations.\n' > "$out/check.txt"
mcopy -i "$out/input.fat" "$out/check.txt" ::/CHECK.TXT
python3 tools/build-ntfs.py "$out"
C=(-O1 -g -ffunction-sections -fdata-sections -fno-builtin -DTMC_NTFS_HOST -Iinclude -Iarch)
S=(tests/diskedit_host.c storage/diskedit.c storage/ntfs_resize.c storage/partition.c storage/maintenance.c storage/native_resize.c storage/native_cow.c crypto/sha256.c kernel/string.c)
gcc "${C[@]}" "${S[@]}" "$out/k-native-ntfs.o" -no-pie -Wl,--gc-sections -o "$out/host"
gcc "${C[@]}" -fsanitize=address,undefined -fno-omit-frame-pointer "${S[@]}" "$out/k-native-ntfs.o" -no-pie -Wl,--gc-sections -o "$out/sanitized"
for test in host sanitized;do
 ASAN_OPTIONS=detect_leaks=1 "$out/$test" "$out/input.fat" "$out"
 for scheme in gpt mbr;do
  for size in shrunk grown;do
   fsck.fat -n "$out/$scheme-$size.fat"
   mtype -i "$out/$scheme-$size.fat" ::/CHECK.TXT > "$out/readback.txt"
   cmp "$out/check.txt" "$out/readback.txt"
  done
 done
 echo "PASS $test: independent fsck.fat and exact file readback on both schemes/sizes"
done
