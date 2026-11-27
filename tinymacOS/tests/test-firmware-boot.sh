#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/..";source tools/env.sh
out=$(mktemp -d /var/tmp/tmc-firmware-boot-XXXXXXXX);trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fshort-wchar -DGNU_EFI_USE_MS_ABI -fno-builtin -I"$EFIINC" -I"$EFIINC/x86_64" -Iinclude "${flags[@]}" tests/firmware_boot_host.c kernel/string.c -o "$out/$mode"
 "$out/$mode"
done
