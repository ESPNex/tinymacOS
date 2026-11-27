#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-home-legacy-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch -Inet/include -Inet/lwip/include "${flags[@]}" tests/home_legacy_host.c storage/native_cow.c storage/native_resize.c storage/maintenance.c storage/partition.c crypto/sha256.c kernel/string.c -Wl,--gc-sections -o "$out/$mode"
 "$out/$mode"
done
