#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/localization6_source_contract.py
out=$(mktemp -d /var/tmp/tmc-localization6-host-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch -Iuserspace "${flags[@]}" tests/localization6_host.c kernel/string.c -Wl,--gc-sections -o "$out/$mode"
 "$out/$mode"
done
