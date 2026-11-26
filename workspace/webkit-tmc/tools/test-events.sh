#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=${TMC_EVENTS_TEST_BUILD:-/var/tmp/tmc-webkit-events-tests}
mkdir -p "$OUT"
for mode in normal sanitized;do
 flags=''
 if [ "$mode" = sanitized ];then flags='-fsanitize=address,undefined -fno-omit-frame-pointer';fi
 objects=''
 for source in posix/src/heap.c posix/src/sync.c posix/src/threads.c posix/src/pthread.c posix/src/time.c posix/src/runloop.c tests/threads_host_transport.c tests/cpp_vm_transport.c tests/events_contract.c tests/events_cases.c;do
  name=$(basename "$source").o
  extra='';if [ "$source" = tests/events_cases.c ];then extra="-I$ROOT/posix/pthread-headers";fi
  gcc -std=c11 -O1 -g -Wall -Wextra -Werror -fno-builtin -pthread $flags $extra -I"$ROOT/cpp/include" -I"$ROOT/posix/include" -I"$ROOT/native-abi" -I"$ROOT/tests" -c "$ROOT/$source" -o "$OUT/$name"
  objects="$objects $OUT/$name"
 done
 for source in cpp/src/new.cpp cpp/src/guard.cpp cpp/src/lifecycle.cpp tests/events_cpp_cases.cpp;do
  name=$(basename "$source").o
  g++ -std=c++20 -nostdinc++ -O1 -g -Wall -Wextra -Werror -fno-builtin -fno-exceptions -fno-rtti $flags -I"$ROOT/posix/pthread-headers" -I"$ROOT/cpp/include" -I"$ROOT/posix/include" -I"$ROOT/tests" -c "$ROOT/$source" -o "$OUT/$name"
  objects="$objects $OUT/$name"
 done
 # Only our authored test objects use this private ABI name on the host.
 # Never intercept glibc/ASan's own process-global __cxa_atexit registrations.
 for object in $objects;do
  objcopy --redefine-sym __cxa_atexit=tmc_test_cxa_atexit --redefine-sym __cxa_finalize=tmc_test_cxa_finalize "$object"
 done
 gcc -pthread $flags $objects -o "$OUT/$mode"
 ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$OUT/$mode"
 if readelf -d "$OUT/$mode" | grep NEEDED | grep -q libstdc++;then echo 'Unexpected direct hosted C++ library' >&2;exit 1;fi
 nm "$OUT/$mode" | grep -q ' T tmc_test_cxa_atexit$'
 nm "$OUT/$mode" | grep -q ' T tmc_cpp_run_initializers$'
done
