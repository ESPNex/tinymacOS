#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
D=$(mktemp -d /var/tmp/tmc-drag8-XXXXXXXX)
trap 'rm -rf "$D"' EXIT
python3 tools/tramontana-assets.py "$D" >/dev/null
printf '.section .rodata\n.global icons,systemicons\nicons: .incbin "%s/icons.atlas"\nsystemicons: .incbin "%s/assets/symbols.rgba"\n.section .note.GNU-stack,"",@progbits\n' "$D" "$PWD" > "$D/assets.S"
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch -Iuserspace -Idesktop "${flags[@]}" tests/drag8_host.c desktop/gfx.c kernel/string.c "$D/assets.S" -Wl,--gc-sections -o "$D/test"
 "$D/test"
done
