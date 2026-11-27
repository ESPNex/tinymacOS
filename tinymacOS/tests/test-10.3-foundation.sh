#!/bin/bash
# Host gate only: this script does NOT claim a complete 10.3 release or guest QA.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/test-m0.py
for suite in textedit-open ui-stream display-size m3-acceptance windows-gaps caret filepanel m2-geometry m1-acceptance;do
 bash "tests/test-$suite.sh"
done
python3 tools/check-usb-retirement.py
out=$(mktemp -d /var/tmp/tmc-103-objects-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for src in userspace/ui.c userspace/textedit.c userspace/diskutility.c userspace/monitor.c userspace/notes.c userspace/stickies.c userspace/terminal.c userspace/filepanel.c userspace/installer.c userspace/settings.c userspace/vaultpanel.c userspace/finder.c userspace/calendar.c desktop/main.c desktop/gfx.c desktop/login.c desktop/recovery.c;do
 gcc -c -O2 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -mgeneral-regs-only \
  -Wall -Wextra -Werror=implicit-function-declaration -Wno-sign-compare -Wno-misleading-indentation \
  -Iinclude -Iarch -Iuserspace -Idesktop "$src" -o "$out/${src##*/}.o"
done
echo 'PASS 10.3 foundation host gate only; ISO, guest and hardware acceptance still pending'
