#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/env.sh
out=$(mktemp -d /var/tmp/tmc-boot-default-test-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
C=(-O1 -g -fshort-wchar -DGNU_EFI_USE_MS_ABI -I"$EFIINC" -I"$EFIINC/x86_64" -Iinclude)
gcc "${C[@]}" tests/boot_default.c -o "$out/host"
"$out/host"
gcc "${C[@]}" -fsanitize=address,undefined -fno-omit-frame-pointer tests/boot_default.c -o "$out/sanitized"
ASAN_OPTIONS=detect_leaks=1 "$out/sanitized"
