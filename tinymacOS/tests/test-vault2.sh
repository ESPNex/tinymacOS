#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-vault2-test-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
src=(tests/vault2_host.c crypto/vault2.c crypto/sha256.c crypto/pbkdf.c crypto/monocypher/monocypher.c storage/tmcbfs2.c kernel/string.c)
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fno-builtin -Iinclude "${flags[@]}" "${src[@]}" -o "$out/$mode"
 "$out/$mode" "$out/$mode-vector.bin"
 python3 tests/vault2_independent.py "$out/$mode-vector.bin"
done
