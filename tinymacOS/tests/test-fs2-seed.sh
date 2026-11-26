#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc2-seed-test-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude "${flags[@]}" tests/fs2_seed_host.c storage/tmcbfs2_seed.c storage/tmcbfs2.c storage/native_resize.c storage/maintenance.c kernel/string.c -Wl,--gc-sections -o "$out/$mode"
 "$out/$mode"
done
