#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-animation-host-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
# Derive the independent slow path by disabling only the interior shortcut.
python3 - "$out/reference.c" <<'PY'
from pathlib import Path
import sys
s=Path('desktop/glass.c').read_text();needle='if (ex >= MAX(r, 18) && ey >= MAX(r, 18))'
assert s.count(needle)==1
s=s.replace(needle,'if (0)').replace('void glass(', 'void glass_reference(').replace('void stroke(', 'void reference_stroke(').replace('void shadow(', 'void reference_shadow(').replace('void glass_sheet_backdrop(', 'void reference_sheet_backdrop(')
Path(sys.argv[1]).write_text(s)
PY
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 c=(-O2 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch -Iuserspace -Idesktop "${flags[@]}")
 gcc "${c[@]}" tests/animation_clock_host.c desktop/motion.c kernel/string.c -Wl,--gc-sections -o "$out/clock-$mode"
 "$out/clock-$mode"
 gcc "${c[@]}" tests/window_surface_host.c -Wl,--gc-sections -o "$out/surface-$mode"
 "$out/surface-$mode"
 gcc "${c[@]}" tests/glass_fastpath_host.c desktop/glass.c desktop/gfx.c "$out/reference.c" kernel/string.c -Wl,--gc-sections -o "$out/glass-$mode"
 "$out/glass-$mode"
done
