#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-cpu-sample-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fno-builtin -Iinclude "${flags[@]}" tests/cpu_sample_host.c kernel/string.c -o "$out/$mode"
 "$out/$mode"
 gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude "${flags[@]}" tests/monitor_sample_host.c kernel/string.c -Wl,--gc-sections -o "$out/monitor-$mode"
 "$out/monitor-$mode"
done
