#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-mc8-optics-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 c=(-O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch -Iuserspace -Idesktop "${flags[@]}")
 gcc "${c[@]}" tests/mc8_optics_host.c desktop/gfx.c desktop/glass.c desktop/motion.c kernel/string.c -Wl,--gc-sections -o "$out/optics-$mode"
 "$out/optics-$mode"
 gcc "${c[@]}" tests/mc8_dock_host.c desktop/gfx.c desktop/glass.c kernel/string.c -Wl,--gc-sections -o "$out/dock-$mode"
 "$out/dock-$mode"
done
