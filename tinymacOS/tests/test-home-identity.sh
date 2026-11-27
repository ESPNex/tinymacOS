#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-home-identity-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch "${flags[@]}" tests/home_identity_host.c fs/tmcbfs2_volume.c storage/fs2_convert.c storage/tmcbfs2.c crypto/vault2.c crypto/pbkdf.c crypto/sha256.c crypto/monocypher/monocypher.c kernel/string.c -Wl,--gc-sections -o "$out/$mode"
 "$out/$mode"
done
