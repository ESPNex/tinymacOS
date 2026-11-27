#!/bin/bash
# Host only: real workload / S_INFO / S_PS guest acceptance is separate.
set -euo pipefail
cd "$(dirname "$0")/.."
bash tests/test-cpu-sample.sh
out=$(mktemp -d /var/tmp/tmc-m3-host-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized; do
 flags=(); [[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 for name in m3_account_host m3_memory_host m3_monitor_host; do
  gcc -O1 -g -pthread -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch "${flags[@]}" tests/$name.c kernel/string.c -Wl,--gc-sections -o "$out/$name-$mode"
  "$out/$name-$mode"
 done
 echo "PASS M3 real accounting/allocator $mode"
done
python3 tests/m3_source_contract.py
