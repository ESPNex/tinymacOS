#!/bin/bash
# Loader/firmware-record host gate only; no claim of GeForce/HDMI acceptance.
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/env.sh
out=$(mktemp -d /var/tmp/tmc-gop-modes-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized; do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fshort-wchar -DGNU_EFI_USE_MS_ABI -I"$EFIINC" -I"$EFIINC/x86_64" -Iinclude "${flags[@]}" tests/gop_mode_host.c -o "$out/$mode"
 "$out/$mode"
 gcc -O1 -g -fno-builtin -Iinclude "${flags[@]}" tests/bootconfig.c crypto/sha256.c kernel/string.c -o "$out/bootconfig-$mode"
 "$out/bootconfig-$mode"
done
