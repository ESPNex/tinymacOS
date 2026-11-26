#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=${TMC_TIME_TEST_BUILD:-/var/tmp/tmc-webkit-time-tests}
mkdir -p "$OUT"
for mode in normal sanitized;do
 flags=''
 if [ "$mode" = sanitized ];then flags='-fsanitize=address,undefined -fno-omit-frame-pointer';fi
 gcc -std=c11 -O1 -g -Wall -Wextra -Werror $flags -I"$ROOT/posix/include" -I"$ROOT/native-abi" "$ROOT/posix/src/time.c" "$ROOT/tests/time_contract.c" -o "$OUT/$mode"
 ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$OUT/$mode"
done
