#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-caret-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch -Iuserspace "${flags[@]}" tests/caret_overlay_host.c kernel/string.c -Wl,--gc-sections -o "$out/$mode"
 "$out/$mode"
 for sticky in 0 1;do
  gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch -Iuserspace "${flags[@]}" -DSTICKY=$sticky tests/notebook_caret_host.c kernel/string.c -Wl,--gc-sections -o "$out/notebook-$sticky-$mode"
  "$out/notebook-$sticky-$mode"
 done
 gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch -Iuserspace "${flags[@]}" tests/terminal_clipboard_host.c kernel/clipboard.c userspace/ui.c kernel/string.c -Wl,--gc-sections -o "$out/terminal-$mode"
 "$out/terminal-$mode"
done
