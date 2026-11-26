#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc2-import-adapter-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -ffunction-sections -fdata-sections -fno-builtin -Iinclude -Iarch "${flags[@]}" tests/fs2_import_adapter_host.c fs/tmcbfs2_volume.c storage/tmcbfs2.c crypto/vault2.c crypto/pbkdf.c crypto/sha256.c crypto/monocypher/monocypher.c kernel/string.c -Wl,--gc-sections -o "$out/$mode"
 "$out/$mode"
 echo "PASS external import kernel adapters $mode"
done
