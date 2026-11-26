#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-textedit-open-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch "${flags[@]}" tests/textedit_open_host.c kernel/string.c -Wl,--gc-sections -o "$out/$mode"
 "$out/$mode"
done
