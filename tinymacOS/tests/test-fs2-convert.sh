#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-convert-test-XXXXXXXX);trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fno-builtin -Wall -Wextra -Wno-misleading-indentation -Iinclude "${flags[@]}" tests/fs2_convert_host.c storage/fs2_convert.c storage/tmcbfs2.c crypto/vault2.c crypto/pbkdf.c crypto/sha256.c crypto/monocypher/monocypher.c kernel/string.c -o "$out/$mode"
 "$out/$mode"
 echo "PASS same-extent FS2 conversion $mode"
done
