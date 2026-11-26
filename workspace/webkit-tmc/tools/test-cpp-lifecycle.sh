#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=${TMC_LIFECYCLE_TEST_BUILD:-/var/tmp/tmc-webkit-lifecycle-tests}
mkdir -p "$OUT"
for mode in normal sanitized;do
 flags=''
 if [ "$mode" = sanitized ];then flags='-fsanitize=address,undefined -fno-omit-frame-pointer';fi
 objects=''
 for source in posix/src/heap.c posix/src/sync.c posix/src/threads.c tests/threads_host_transport.c tests/cpp_vm_transport.c tests/lifecycle_contract.c;do
  name=$(basename "$source").o
  gcc -std=c11 -O1 -g -Wall -Wextra -Werror -fno-builtin -pthread $flags -I"$ROOT/cpp/include" -I"$ROOT/posix/include" -I"$ROOT/native-abi" -I"$ROOT/tests" -c "$ROOT/$source" -o "$OUT/$name"
  objects="$objects $OUT/$name"
 done
 for source in cpp/src/new.cpp cpp/src/guard.cpp cpp/src/lifecycle.cpp tests/lifecycle_cases.cpp;do
  name=$(basename "$source").o
  g++ -std=c++20 -nostdinc++ -O1 -g -Wall -Wextra -Werror -fno-builtin -fno-exceptions -fno-rtti $flags -I"$ROOT/cpp/include" -I"$ROOT/posix/include" -I"$ROOT/tests" -c "$ROOT/$source" -o "$OUT/$name"
  objects="$objects $OUT/$name"
 done
 # Only our authored test objects use this private ABI name on the host.
 # Never intercept glibc/ASan's own process-global __cxa_atexit registrations.
 for object in $objects;do
  objcopy --redefine-sym __cxa_atexit=tmc_test_cxa_atexit --redefine-sym __cxa_finalize=tmc_test_cxa_finalize "$object"
 done
 # Keep sanitizer .init_array.00099 automatic; manually run ONLY the C++
 # consumer's ordinary global constructor section through our runtime.
 objcopy --rename-section .init_array=tmc_test_init "$OUT/lifecycle_cases.cpp.o"
 gcc -pthread $flags $objects -o "$OUT/$mode"
 ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$OUT/$mode"
 if readelf -d "$OUT/$mode" | grep NEEDED | grep -q libstdc++;then echo 'Unexpected direct hosted C++ library' >&2;exit 1;fi
 nm "$OUT/$mode" | grep -q ' T tmc_test_cxa_atexit$'
 nm "$OUT/$mode" | grep -q ' T tmc_cpp_run_initializers$'
done
