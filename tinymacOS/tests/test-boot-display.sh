#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-boot-display-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
C=(-O1 -g -fno-builtin -ffunction-sections -fdata-sections -Itests/boot_display_support -Iinclude -Iarch -pthread -Wl,--gc-sections)
for mode in normal sanitized;do
 extra=();[[ $mode != sanitized ]] || extra=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc "${C[@]}" "${extra[@]}" tests/boot_display_host.c kernel/string.c -o "$out/$mode"
 "$out/$mode"
 echo "PASS boot display $mode"
done
