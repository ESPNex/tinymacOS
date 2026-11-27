#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/check-legacy-retirement.py
out=$(mktemp -d /var/tmp/tmc-legacy-retirement-XXXXXX);trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch "${flags[@]}" tests/legacy_retirement_host.c storage/tmcbfs2_seed.c storage/tmcbfs2.c crypto/sha256.c kernel/string.c -Wl,--gc-sections -o "$out/$mode"
 UBSAN_OPTIONS=halt_on_error=1 "$out/$mode"
done
