#!/bin/bash
# Actual native fs driver, allocator and resize engine on disposable memory.
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-native-host-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
C=(-O1 -g -ffunction-sections -fdata-sections -fno-builtin -Iinclude -Iarch -Inet/include -Inet/lwip/include)
S=(tests/native_resize_host.c storage/native_resize.c storage/native_cow.c storage/maintenance.c storage/partition.c kernel/string.c crypto/sha256.c)
gcc "${C[@]}" "${S[@]}" -Wl,--gc-sections -o "$out/normal"
"$out/normal"
gcc "${C[@]}" -fsanitize=address,undefined -fno-omit-frame-pointer "${S[@]}" -Wl,--gc-sections -o "$out/sanitized"
ASAN_OPTIONS=detect_leaks=1 "$out/sanitized"
