#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-browser-pages-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 for test in finder_pages_host timemachine_pages_host;do
  units=();[[ $test != finder_pages_host ]] || units=(userspace/ui.c)
  gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch "${flags[@]}" tests/$test.c "${units[@]}" kernel/string.c -Wl,--gc-sections -o "$out/$test-$mode"
  "$out/$test-$mode"
 done
done
bash tests/test-filepanel.sh
