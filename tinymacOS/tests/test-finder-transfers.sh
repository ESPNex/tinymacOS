#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-finder-transfers-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 common=(-O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch -Iuserspace)
 gcc "${common[@]}" "${flags[@]}" tests/fileops_host.c storage/tmcbfs2.c kernel/string.c -Wl,--gc-sections -o "$out/fileops-$mode"
 "$out/fileops-$mode"
 gcc "${common[@]}" "${flags[@]}" tests/terminal_clipboard_host.c kernel/clipboard.c userspace/ui.c kernel/string.c -Wl,--gc-sections -o "$out/terminal-$mode"
 "$out/terminal-$mode"
 echo "PASS Finder/Terminal transfers $mode"
done
bash tests/test-browser-pages.sh
