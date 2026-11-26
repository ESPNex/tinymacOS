#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=${TMC_CPP_TEST_BUILD:-/var/tmp/tmc-webkit-cpp-tests}
mkdir -p "$OUT"
for mode in normal sanitized;do
 flags=''
 if [ "$mode" = sanitized ];then flags='-fsanitize=address,undefined -fno-omit-frame-pointer';fi
 for source in posix/src/heap.c posix/src/sync.c posix/src/threads.c tests/threads_host_transport.c tests/cpp_vm_transport.c tests/cpp_contract.c;do
  gcc -std=c11 -O1 -g -Wall -Wextra -Werror -fno-builtin -pthread $flags -I"$ROOT/posix/include" -I"$ROOT/native-abi" -I"$ROOT/tests" -c "$ROOT/$source" -o "$OUT/$(basename "$source").o"
 done
 for source in cpp/src/new.cpp cpp/src/guard.cpp tests/cpp_cases.cpp;do
  g++ -std=c++20 -nostdinc++ -O1 -g -Wall -Wextra -Werror -fno-builtin -fno-exceptions -fno-rtti $flags -I"$ROOT/cpp/include" -I"$ROOT/posix/include" -I"$ROOT/tests" -c "$ROOT/$source" -o "$OUT/$(basename "$source").o"
 done
 # GCC (not g++) deliberately avoids a hosted libstdc++ and its competing ABI.
 gcc -pthread $flags "$OUT/heap.c.o" "$OUT/sync.c.o" "$OUT/threads.c.o" "$OUT/threads_host_transport.c.o" "$OUT/cpp_vm_transport.c.o" "$OUT/cpp_contract.c.o" "$OUT/new.cpp.o" "$OUT/guard.cpp.o" "$OUT/cpp_cases.cpp.o" -o "$OUT/$mode"
 ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$OUT/$mode"
 if readelf -d "$OUT/$mode" | grep NEEDED | grep -q libstdc++;then echo 'Unexpected direct hosted C++ standard library' >&2;exit 1;fi
 if [ "$mode" = normal ] && ldd "$OUT/$mode" | grep -q libstdc++;then echo 'Unexpected hosted C++ dependency in normal test' >&2;exit 1;fi
 # UBSan itself loads libstdc++ for diagnostics on this host. Strong executable
 # definitions (not that library) must still provide our allocation/guard ABI.
 nm "$OUT/$mode" | grep -q ' T __cxa_guard_acquire$'
 nm "$OUT/$mode" | grep -q ' T _Znwm$'
done
