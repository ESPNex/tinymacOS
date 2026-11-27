#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-apps5-host-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 for app in 1 2 3;do
  gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch -Iuserspace "${flags[@]}" -DAPP_CASE=$app tests/apps5_host.c kernel/string.c -Wl,--gc-sections -o "$out/$app-$mode"
  "$out/$app-$mode"
 done
done
