#!/bin/sh
# Requires gcc, Python 3 and PyNaCl (libsodium). No guest or real disk attached.
set -eu
cd "$(dirname "$0")/.."
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
src='tests/vault_host.c crypto/vault.c crypto/sha256.c crypto/pbkdf.c crypto/monocypher/monocypher.c'
gcc -O2 -Iinclude $src -o "$out/full"
"$out/full" "$out/vector.bin"
python3 tests/vault_independent.py "$out/vector.bin"
# The smaller fixture also enumerates interruption points during conversion.
gcc -O0 -g -Iinclude -DFV_PAYLOAD=64 -fsanitize=address,undefined -fno-omit-frame-pointer $src -o "$out/faults"
"$out/faults" "$out/small-vector.bin"
