#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
D=$(mktemp -d /var/tmp/tmc-image8-XXXXXXXX)
trap 'rm -rf "$D"' EXIT
mkdir "$D/source"
printf 'IMAGE8_ISO_CONTENT\n' > "$D/source/proof.txt"
xorriso -as mkisofs -quiet -o "$D/test.iso" "$D/source"
xorriso -osirrox on -indev "${TMC_IMAGE8_ISO:-/var/tmp/tmc-block8-candidate/tinymacOS-10.2.6-x86_64.iso}" -extract /Recovery/BaseSystem.dmg "$D/BaseSystem.dmg" >/dev/null 2>&1
for opt in -O1 -O2;do
 gcc -std=gnu11 "$opt" -g -fno-builtin -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -Iinclude tests/image8_host.c storage/image8.c storage/iso9660.c kernel/string.c -o "$D/test"
 ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$D/test" iso "$D/test.iso"
 ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$D/test" dmg "$D/BaseSystem.dmg"
done
