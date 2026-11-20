#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=${TMC_TEST_BUILD:-/var/tmp/tmc-webkit-backend-tests}
mkdir -p "$OUT"
gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -pthread \
 -I"$ROOT/posix/include" -I"$ROOT/native-abi" \
 "$ROOT/tests/sync_contract.c" "$ROOT/posix/src/sync.c" -o "$OUT/sync-contract"
"$OUT/sync-contract"
