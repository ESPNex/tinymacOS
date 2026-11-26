#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-usb-retry-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized; do
 flags=();[[ $mode != sanitized ]]||flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 for kind in 1 2 3; do
  gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch -DHOST_KIND=$kind "${flags[@]}" tests/usb_retry_native.c -Wl,--gc-sections -o "$out/$kind-$mode"
  "$out/$kind-$mode"
  echo "PASS native adapter recovery kind=$kind mode=$mode"
 done
done
