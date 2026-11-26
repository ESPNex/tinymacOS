#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-windows-gap-XXXXXXXX);trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude "${flags[@]}" tests/windows_gpt_gaps.c storage/partition.c storage/maintenance.c kernel/string.c -Wl,--gc-sections -o "$out/$mode"
 "$out/$mode"
done
