#!/bin/bash
# Host gate only: this script does NOT claim a complete 10.3 release or guest QA.
set -euo pipefail
cd "$(dirname "$0")/.."
for suite in textedit-open ui-stream display-size cpu-sample windows-gaps;do
 bash "tests/test-$suite.sh"
done
python3 tools/check-usb-retirement.py
out=$(mktemp -d /var/tmp/tmc-103-objects-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for src in userspace/ui.c userspace/textedit.c userspace/diskutility.c userspace/monitor.c;do
 gcc -c -O2 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -mgeneral-regs-only \
  -Wall -Wextra -Werror=implicit-function-declaration -Wno-sign-compare -Wno-misleading-indentation \
  -Iinclude -Iarch -Iuserspace "$src" -o "$out/${src##*/}.o"
done
echo 'PASS 10.3 foundation host gate only; ISO, guest and hardware acceptance still pending'
