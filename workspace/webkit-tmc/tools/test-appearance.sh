#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=/var/tmp/tmc-webkit-appearance-tests
mkdir -p "$OUT"
gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
 -I"$ROOT/native-abi" -I"$ROOT/posix/include" -I"$ROOT/platform/include" \
 "$ROOT/platform/src/appearance.c" "$ROOT/tests/appearance_contract.c" -o "$OUT/test-appearance"
"$OUT/test-appearance"
