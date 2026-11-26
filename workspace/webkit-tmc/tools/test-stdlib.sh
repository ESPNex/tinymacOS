#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${TMC_STDLIB_TEST_BUILD:-/var/tmp/tmc-webkit-stdlib-tests}
mkdir -p "$OUT"
python3 "$ROOT/tools/prepare-dtoa.py" "$OUT/port"
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 objects=()
 for source in posix/src/heap.c posix/src/sync.c posix/src/threads.c posix/src/time.c posix/src/runloop.c tests/threads_host_transport.c tests/cpp_vm_transport.c tests/stdlib_contract.c stdlib/src/bytes.c;do
  name=$(basename "$source").o
  gcc -std=c11 -O1 -g -Wall -Wextra -Werror -fno-builtin -pthread "${flags[@]}" -I"$ROOT/posix/include" -I"$ROOT/native-abi" -I"$ROOT/tests" -c "$ROOT/$source" -o "$OUT/$name"
  objects+=("$OUT/$name")
 done
 for source in cpp/src/new.cpp cpp/src/guard.cpp cpp/src/lifecycle.cpp;do
  name=$(basename "$source").o
  g++ -std=c++20 -nostdinc++ -O1 -g -Wall -Wextra -Werror -fno-builtin -fno-exceptions -fno-rtti "${flags[@]}" -I"$ROOT/cpp/include" -I"$ROOT/posix/include" -c "$ROOT/$source" -o "$OUT/$name"
  objects+=("$OUT/$name")
 done
 # Real libstdc++ HEADER templates: default hosted headers, but no hosted C++
 # binary library. Do NOT call this a complete freestanding standard library.
 for source in "$ROOT/stdlib/src/failure.cpp" "$ROOT/stdlib/src/math.cpp" "$ROOT/tests/stdlib_cases.cpp" "$OUT/port/wtf/dtoa/"*.cc;do
  name=$(basename "$source").o
  g++ -std=c++20 -O2 -g -Wall -Wextra -Werror -Wno-sign-compare -D_GLIBCXX_ASSERTIONS -fno-builtin -fno-exceptions -fno-rtti -mno-avx "${flags[@]}" -I"$OUT/port" -I"$ROOT/stdlib/include" -iquote "$ROOT/cpp/include" -I"$ROOT/posix/include" -I"$ROOT/tests" -MD -MF "$OUT/stdlib-$name.d" -c "$source" -o "$OUT/$name"
  objects+=("$OUT/$name")
 done
 for object in "${objects[@]}";do
  objcopy --redefine-sym __cxa_atexit=tmc_test_cxa_atexit --redefine-sym __cxa_finalize=tmc_test_cxa_finalize --redefine-sym abort=tmc_stdlib_abort --redefine-sym ceil=tmc_stdlib_ceil --redefine-sym strnlen=tmc_stdlib_strnlen --redefine-sym memchr=tmc_stdlib_memchr "$object"
 done
 gcc -pthread "${flags[@]}" "${objects[@]}" -o "$OUT/$mode"
 if [[ $mode == normal ]];then
  # Replay the original negative case with ONLY original strtod.cc replacing
  # its patched object, then require the corrected build to match binary64.
  g++ -std=c++20 -O2 -fno-exceptions -fno-rtti -fno-builtin -I"$OUT/port" -I"$ROOT/stdlib/include" -c "$ROOT/vendor/webkit-dtoa/strtod.cc" -o "$OUT/original-strtod.o"
  objcopy --redefine-sym abort=tmc_stdlib_abort --redefine-sym ceil=tmc_stdlib_ceil --redefine-sym strnlen=tmc_stdlib_strnlen --redefine-sym memchr=tmc_stdlib_memchr "$OUT/original-strtod.o"
  numeric=()
  for name in bignum-dtoa.cc bignum.cc cached-powers.cc diy-fp.cc double-conversion.cc fast-dtoa.cc fixed-dtoa.cc math.cpp failure.cpp bytes.c;do numeric+=("$OUT/$name.o");done
  for version in original patched;do
   extra=();strtod="$OUT/strtod.cc.o";if [[ $version == original ]];then extra=(-DEXPECT_UPSTREAM_SIGNED_BUG);strtod="$OUT/original-strtod.o";fi
   g++ -std=c++20 -O2 -fno-exceptions -fno-rtti -I"$OUT/port" -I"$ROOT/stdlib/include" "${extra[@]}" -c "$ROOT/tests/dtoa_regression.cpp" -o "$OUT/replay.o"
   gcc "$OUT/replay.o" "$strtod" "${numeric[@]}" -o "$OUT/replay-$version"
   "$OUT/replay-$version"
  done
 fi
 ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$OUT/$mode"
 if readelf -d "$OUT/$mode" | grep NEEDED | grep -q libstdc++;then echo 'Unexpected direct libstdc++ binary link' >&2;exit 1;fi
 if [[ $mode == normal ]] && ldd "$OUT/$mode" | grep -q libstdc++;then echo 'Unexpected hosted C++ dependency' >&2;exit 1;fi
done
