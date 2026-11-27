#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-desktop4-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
python3 - <<'PY'
from pathlib import Path
import hashlib,json
p=json.loads(Path('tests/fixtures/m0-contract.json').read_text())['macroblock4_input_modifier_audit']
s=Path('drivers/input.c').read_text();new='.type = EV_KEY, .a = v, .b = ctrl, .c = shift, .d = m & 255';assert s.count(new)==1
assert hashlib.sha256(s.replace(new,'.type = EV_KEY, .a = v, .b = ctrl, .c = shift').encode()).hexdigest()==p['before_sha256']
print('PASS canonical input audit: single Event.d assignment is the sole production driver delta')
PY
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 c=(-O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch -Iuserspace -Idesktop "${flags[@]}")
 for test in shortcuts4_host desktop4_host;do
  gcc "${c[@]}" "tests/$test.c" kernel/string.c -Wl,--gc-sections -o "$out/$test-$mode"
  "$out/$test-$mode"
 done
 for field in 1 2 3 4 5 6 7;do
  gcc "${c[@]}" -DFIELD_CASE=$field tests/fields4_host.c userspace/ui.c kernel/string.c -Wl,--gc-sections -o "$out/field-$field-$mode"
  "$out/field-$field-$mode"
 done
done
