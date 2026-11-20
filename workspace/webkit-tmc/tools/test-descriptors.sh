#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=${TMC_TEST_BUILD:-/var/tmp/tmc-webkit-backend-tests}
mkdir -p "$OUT"
gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
 -I"$ROOT/posix/include" -I"$ROOT/native-abi" \
 "$ROOT/tests/descriptors_contract.c" "$ROOT/posix/src/descriptors.c" \
 "$ROOT/posix/src/network.c" "$ROOT/posix/src/clock_entropy.c" -o "$OUT/descriptors-contract"
"$OUT/descriptors-contract"
