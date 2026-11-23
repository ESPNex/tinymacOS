#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-legacy-input-host.XXXXXXXX)
trap 'rm -rf "$out"' EXIT
C='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Wno-misleading-indentation -Iinclude'
gcc $C tests/ps2.c -o "$out/ps2";"$out/ps2"
# Execute pinned Linux Boot mouse alongside native HCD tests, with host-only OS imports.
LX='tests/linux_usb_host_runtime.c compat/linux/usb_input.c vendor/linux-v6.12/usbmouse.c'
C="$C -Icompat/linux/include -Wno-unused-parameter -Wno-misleading-indentation"

# Host transport substitutes only kernel I/O imports and the PIT wait. Actual
# usb_input_enumerate/report code is executed; it is not a DMA/HCD hardware test.
python3 - "$out/usbinput-test.c" <<'PY'
from pathlib import Path
import sys
p=Path('drivers/usbinput.c').resolve();s=p.read_text();s=s.replace('#include "../kernel/k.h"','#include "'+str(p.parents[1]/'tests/input_test_support.h')+'"')
a=s.index('void usb_delay_ms(');b=s.index('int usb_input_enumerate(',a)
s=s[:a]+'void usb_delay_ms(unsigned ms){(void)ms;}\n'+s[b:]
Path(sys.argv[1]).write_text(s)
PY
gcc $C -DTMC_USBINPUT_TEST_SOURCE=\""$out/usbinput-test.c"\" tests/usbinput_contract.c $LX drivers/hidconfig.c drivers/hidtablet.c -o "$out/usbinput";"$out/usbinput"
