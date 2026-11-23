#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-input-host.XXXXXXXX)
trap 'rm -rf "$out"' EXIT
C='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Iinclude'
# Execute pinned Linux Boot mouse alongside native HCD tests, with host-only OS imports.
LX='tests/linux_usb_host_runtime.c tests/linux_hid_runtime.c compat/linux/hid_parser.c compat/linux/usb_input.c compat/linux/usb_keyboard.c vendor/linux-v6.12/urb.c vendor/linux-v6.12/usbmouse.c'
C="$C -Icompat/linux/include -Wno-unused-parameter -Wno-misleading-indentation -Wno-sign-compare"

gcc $C tests/input_maintenance.c $LX drivers/hidconfig.c compat/linux/usb_config.c tests/linux_config_runtime.c drivers/hidtablet.c -o "$out/queue"
"$out/queue"
# Substitute only the MMIO notification in the host transport. No OS source edit.
python3 - "$out/xhci-test.c" <<'PY'
from pathlib import Path
import sys
p=Path('drivers/xhci.c').resolve();s=p.read_text()
assert s.count('db[s]=ep;')==1
s=s.replace('db[s]=ep;','db[s]=ep;test_doorbell(s,ep);')
s=s.replace('"../tests/input_test_support.h"','"'+str(p.parents[1]/'tests/input_test_support.h')+'"')
s=s.replace('"../kernel/k.h"','"'+str(p.parents[1]/'kernel/k.h')+'"')
Path(sys.argv[1]).write_text(s)
PY
gcc $C -DTMC_XHCI_TEST_SOURCE=\""$out/xhci-test.c"\" tests/input_enumeration.c $LX drivers/hidconfig.c compat/linux/usb_config.c tests/linux_config_runtime.c drivers/hidtablet.c -o "$out/enumeration"
"$out/enumeration"
for t in hidconfig relative_pointer; do gcc $C "tests/$t.c" drivers/hidconfig.c compat/linux/usb_config.c tests/linux_config_runtime.c drivers/hidtablet.c -o "$out/$t";"$out/$t";done

gcc $C -DTMC_XHCI_TEST_SOURCE=\""$out/xhci-test.c"\" tests/xhci_hubs.c $LX drivers/hidconfig.c compat/linux/usb_config.c tests/linux_config_runtime.c drivers/hidtablet.c -o "$out/hubs"
"$out/hubs"
gcc $C tests/usbroute.c -o "$out/route"
"$out/route"

gcc $C tests/keyboard_it.c -o "$out/keyboard-it";"$out/keyboard-it"

gcc $C tests/recovery_usb.c drivers/hidtablet.c -o "$out/recovery-usb"
"$out/recovery-usb"

python3 tests/openduet_input.py

# Actual firmware-only Recovery consumer; no obsolete native-PS/2 fixture.
EFI_HEADERS=${EFIINC:-${TOOLROOT:-}/usr/include/efi}
gcc $C -fshort-wchar -DGNU_EFI_USE_MS_ABI -I"$EFI_HEADERS" -I"$EFI_HEADERS/x86_64" tests/firmware_keyboard_transport.c -o "$out/recovery-keyboard"
"$out/recovery-keyboard"
gcc $C tests/firmware_keyboard.c -o "$out/firmware-keyboard";"$out/firmware-keyboard"
python3 tools/linux-source-contract.py
gcc $C tests/linux_ehci_qtd.c drivers/linux_ehci_qtd.c -o "$out/linux-ehci-qtd";"$out/linux-ehci-qtd"

# Direct upstream probe/IRQ/URB/lifetime and POSIX formatting contracts.
gcc $C -no-pie -pthread tests/linux_usbmouse.c compat/linux/usb_keyboard.c vendor/linux-v6.12/urb.c vendor/linux-v6.12/usbmouse.c -o "$out/linux-usbmouse";"$out/linux-usbmouse"

# Complete upstream Boot keyboard + actual Italian input + native EP0 adapter.
gcc $C -fno-pie -no-pie -pthread tests/linux_usbkbd.c compat/linux/usb_keyboard.c vendor/linux-v6.12/urb.c vendor/linux-v6.12/usbmouse.c -o "$out/linux-usbkbd";"$out/linux-usbkbd"

# Entire original Linux URB core, all anchors/lifetime/cancel/timeout APIs.
gcc $C -fno-pie -no-pie -pthread tests/linux_urb.c compat/linux/usb_keyboard.c vendor/linux-v6.12/urb.c vendor/linux-v6.12/usbmouse.c -o "$out/linux-urb";"$out/linux-urb"

# Two actual Linux mice: aggregate buttons and real error/cancel lifetimes.
gcc $C -fno-pie -no-pie -pthread tests/linux_mouse_lifecycle.c compat/linux/usb_keyboard.c vendor/linux-v6.12/urb.c vendor/linux-v6.12/usbmouse.c -o "$out/linux-mouse-lifecycle";"$out/linux-mouse-lifecycle"

# Complete original Linux PS/2 command/ACK core, not native PS/2 decoders.
gcc $C -fno-builtin -fno-pie -no-pie -pthread tests/linux_ps2.c compat/linux/ps2_protocol.c compat/linux/usb_keyboard.c vendor/linux-v6.12/urb.c vendor/linux-v6.12/usbmouse.c -o "$out/linux-ps2";"$out/linux-ps2"

# Original Linux psmouse packet/detect functions with actual libps2 commands.
gcc $C -fno-builtin -fno-pie -no-pie -pthread tests/linux_psmouse.c compat/linux/ps2_protocol.c compat/linux/psmouse_input.c compat/linux/usb_keyboard.c vendor/linux-v6.12/urb.c vendor/linux-v6.12/usbmouse.c -o "$out/linux-psmouse";"$out/linux-psmouse"

# Complete unchanged USB configuration/endpoint/BOS core and real adapter.
gcc $C -fno-builtin -fno-pie -no-pie tests/linux_config.c tests/linux_config_runtime.c drivers/hidconfig.c -o "$out/linux-config";"$out/linux-config"

# Genuine Linux HID descriptor parser functions, isolated from Boot/HCD ABI.
gcc $C -fno-builtin -fno-pie -no-pie -pthread tests/linux_hid_parser.c tests/linux_hid_runtime.c compat/linux/hid_parser.c drivers/hidtablet.c -o "$out/linux-hid-parser";"$out/linux-hid-parser"
