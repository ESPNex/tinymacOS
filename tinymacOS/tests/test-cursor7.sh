#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-cursor7-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
python3 tests/interaction7_source_contract.py
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 for name in cursor7_host cursor7_context_host resize7_queue_host;do
 gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch -Iuserspace "${flags[@]}" tests/$name.c kernel/string.c -Wl,--gc-sections -o "$out/$name-$mode"
 "$out/$name-$mode"
 done
 gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch -Iuserspace -Idesktop "${flags[@]}" tests/cursor7_pixels_host.c desktop/gfx.c kernel/string.c -Wl,--gc-sections -o "$out/pixels-$mode"
 "$out/pixels-$mode"
done
