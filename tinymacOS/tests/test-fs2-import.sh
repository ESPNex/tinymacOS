#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc2-import-test-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fno-builtin -Iinclude "${flags[@]}" tests/fs2_import_host.c storage/tmcbfs2.c kernel/string.c -o "$out/$mode"
 "$out/$mode"
 echo "PASS external checkpoint import $mode"
done
