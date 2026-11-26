#!/bin/sh
# Compile only: no libc, executable or WebKit engine is linked by this script.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
HEADERS=${TMC_LIBC_HEADERS:-$ROOT/posix/libc-headers}
OUT=${TMC_BACKEND_BUILD:-/var/tmp/tmc-webkit-backend}
mkdir -p "$OUT"
for f in native_syscall network clock_entropy poll descriptors vm tls errno sync threads heap libc_alloc; do
 gcc -std=gnu11 -nostdinc -isystem "$(gcc -print-file-name=include)" \
 -I"$HEADERS" -I"$ROOT/native-abi" -I"$ROOT/posix/include" \
 -ffreestanding -fno-stack-protector -fno-builtin -mno-red-zone \
 -Wall -Wextra -Werror -c "$ROOT/posix/src/$f.c" -o "$OUT/$f.o"
done
for f in new guard;do
 g++ -std=c++20 -nostdinc -nostdinc++ -isystem "$(gcc -print-file-name=include)" \
 -I"$HEADERS" -I"$ROOT/native-abi" -I"$ROOT/posix/include" -I"$ROOT/cpp/include" \
 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-builtin -mno-red-zone \
 -Wall -Wextra -Werror -c "$ROOT/cpp/src/$f.cpp" -o "$OUT/cpp-$f.o"
done
# Inspect only the known freshly compiled objects, never stale directory contents.
ld -r "$OUT/native_syscall.o" "$OUT/network.o" "$OUT/clock_entropy.o" "$OUT/poll.o" "$OUT/descriptors.o" "$OUT/vm.o" "$OUT/tls.o" "$OUT/errno.o" "$OUT/sync.o" "$OUT/threads.o" "$OUT/heap.o" "$OUT/libc_alloc.o" "$OUT/cpp-new.o" "$OUT/cpp-guard.o" -o "$OUT/backend-combined.o"
if nm -u "$OUT/backend-combined.o" | grep -E ' (errno|__errno_location)$';then
 echo 'Unresolved target errno ABI' >&2;exit 1
fi
nm -u "$OUT/backend-combined.o"
printf '%s\n' 'Compile-only PASS. Target errno resolves to caller-attached TCB; no complete target libc has been linked.'
