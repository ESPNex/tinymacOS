#!/bin/sh
# Compile only: no libc, executable or WebKit engine is linked by this script.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
HEADERS=${TMC_LIBC_HEADERS:-$ROOT/posix/libc-headers}
OUT=${TMC_BACKEND_BUILD:-/var/tmp/tmc-webkit-backend}
mkdir -p "$OUT"
for f in native_syscall network clock_entropy poll descriptors vm tls; do
 gcc -std=gnu11 -nostdinc -isystem "$(gcc -print-file-name=include)" \
 -I"$HEADERS" -I"$ROOT/native-abi" -I"$ROOT/posix/include" \
 -ffreestanding -fno-stack-protector -fno-builtin -mno-red-zone \
 -Wall -Wextra -Werror -c "$ROOT/posix/src/$f.c" -o "$OUT/$f.o"
done
nm -u "$OUT/"*.o
printf '%s\n' 'Compile-only PASS. errno remains unresolved: no target libc has been linked.'
