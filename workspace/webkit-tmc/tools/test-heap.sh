#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=${TMC_HEAP_TEST_BUILD:-/var/tmp/tmc-webkit-heap-tests}
mkdir -p "$OUT"
for mode in normal sanitized;do
 flags=''
 if [ "$mode" = sanitized ];then flags='-fsanitize=address,undefined -fno-omit-frame-pointer';fi
 gcc -std=c11 -O1 -g -Wall -Wextra -Werror -fno-builtin $flags -pthread \
  -I"$ROOT/posix/include" -I"$ROOT/native-abi" \
  "$ROOT/tests/heap_contract.c" "$ROOT/posix/src/heap.c" -o "$OUT/$mode"
 ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$OUT/$mode"
done
