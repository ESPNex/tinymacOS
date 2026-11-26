#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc2-core-host-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fno-builtin -Iinclude "${flags[@]}" tests/tmcbfs2_host.c storage/tmcbfs2.c kernel/string.c -o "$out/$mode"
 "$out/$mode"
 echo "PASS experimental TMCBFS2 core $mode (not a shipping integration claim)"
done
