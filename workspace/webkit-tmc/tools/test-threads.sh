#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=${TMC_THREADS_TEST_BUILD:-/var/tmp/tmc-webkit-threads-tests}
mkdir -p "$OUT"
for f in "$ROOT/posix/src/threads.c" "$ROOT/posix/src/sync.c" "$ROOT/tests/threads_contract.c" "$ROOT/tests/threads_host_transport.c";do
 gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -pthread \
 -I"$ROOT/posix/include" -I"$ROOT/native-abi" -c "$f" -o "$OUT/$(basename "$f").o"
done
g++ -std=c++20 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -fsanitize=address,undefined -g \
 -I"$ROOT/posix/include" -c "$ROOT/tests/threads_cpp_smoke.cpp" -o "$OUT/cpp.o"
g++ -pthread -fsanitize=address,undefined "$OUT/threads.c.o" "$OUT/sync.c.o" "$OUT/threads_contract.c.o" "$OUT/threads_host_transport.c.o" "$OUT/cpp.o" -o "$OUT/threads-contract"
"$OUT/threads-contract"
