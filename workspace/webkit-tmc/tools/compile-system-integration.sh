#!/bin/sh
# Compile modified target sources only. Never creates an ISO or a release.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SYSTEM=/home/user/tinymacOS
OUT=/var/tmp/tmc-webkit-system-integration
mkdir -p "$OUT"
cd "$SYSTEM"
for f in net syscall memory uservm; do
 gcc -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin \
  -fno-asynchronous-unwind-tables -fno-unwind-tables -mno-red-zone -mgeneral-regs-only \
  -fno-ident -Wall -Wextra -Wno-misleading-indentation -Wno-sign-compare \
  -Iinclude -Iarch -Inet/include -Inet/lwip/include -fno-pie \
  -c "kernel/$f.c" -o "$OUT/$f.o"
done
gcc -O2 -ffreestanding -fno-stack-protector -fno-builtin -mno-red-zone \
 -mgeneral-regs-only -fpie -fno-plt -Wall -Wextra -Wno-sign-compare -Wno-misleading-indentation \
 -Iinclude -Iarch -Iuserspace -Idesktop -c desktop/gfx.c -o "$OUT/gfx.o"
gcc -std=gnu11 -nostdinc -isystem "$(gcc -print-file-name=include)" \
 -I"$ROOT/posix/libc-headers" -I"$ROOT/native-abi" -I"$ROOT/posix/include" -I"$ROOT/platform/include" \
 -ffreestanding -fno-stack-protector -fno-builtin -mno-red-zone -Wall -Wextra -Werror \
 -c "$ROOT/platform/src/appearance.c" -o "$OUT/appearance.o"
printf '%s\n' 'PASS: target object compilation: kernel network, syscall dispatch, compositor and appearance adapter. No executable linked, no ISO produced.'
