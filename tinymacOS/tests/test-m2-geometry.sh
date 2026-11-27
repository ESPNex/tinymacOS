#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-m2-geometry-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 for test in m2_geometry_host diskutility_extent_host;do
  gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch -Iuserspace "${flags[@]}" "tests/$test.c" kernel/string.c -Wl,--gc-sections -o "$out/$test-$mode"
  "$out/$test-$mode"
 done
done
