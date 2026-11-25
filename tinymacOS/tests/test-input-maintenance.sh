#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-input-host.XXXXXXXX)
trap 'rm -rf "$out"' EXIT
C='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Iinclude'
# Execute pinned Linux Boot mouse alongside native HCD tests, with host-only OS imports.
LX='tests/linux_usb_host_runtime.c tests/linux_hid_runtime.c compat/linux/hid_parser.c compat/linux/usb_generic.c compat/linux/usb_input.c compat/linux/usb_keyboard.c vendor/linux-v6.12/urb.c vendor/linux-v6.12/usbmouse.c'
C="$C -Icompat/linux/include -Wno-unused-parameter -Wno-misleading-indentation -Wno-sign-compare"

gcc $C tests/input_maintenance.c $LX compat/linux/usb_hub.c drivers/hidconfig.c compat/linux/usb_config.c tests/linux_config_runtime.c drivers/hidtablet.c -o "$out/queue"
"$out/queue"
# Actual shared enumeration/class adapter; scripted EP0, not HCD proof.
python3 - "$out/usbinput-test.c" <<'PYCODE'
from pathlib import Path
import sys
p=Path('drivers/usbinput.c').resolve();s=p.read_text().replace('#include "../kernel/k.h"','#include "'+str(p.parents[1]/'tests/input_test_support.h')+'"')
a=s.index('void usb_delay_ms(');b=s.index('static int linux_keyboard_control(',a)
s=s[:a]+'void usb_delay_ms(unsigned ms){(void)ms;}\n'+s[b:]
Path(sys.argv[1]).write_text(s)
PYCODE
gcc $C -DTMC_USBINPUT_TEST_SOURCE=\""$out/usbinput-test.c"\" tests/usbinput_contract.c $LX compat/linux/usb_hub.c drivers/hidconfig.c compat/linux/usb_config.c tests/linux_config_runtime.c drivers/hidtablet.c -o "$out/enumeration"
"$out/enumeration"
for t in hidconfig relative_pointer; do gcc $C "tests/$t.c" drivers/hidconfig.c compat/linux/usb_config.c tests/linux_config_runtime.c drivers/hidtablet.c -o "$out/$t";"$out/$t";done
# Both optimizations execute original rings, commands, hub contexts and DMA.
sh tests/test-linux-xhci.sh
gcc $C tests/usbroute.c -o "$out/route"
"$out/route"

gcc $C tests/keyboard_it.c -o "$out/keyboard-it";"$out/keyboard-it"

gcc $C tests/recovery_usb.c drivers/hidtablet.c -o "$out/recovery-usb"
"$out/recovery-usb"

python3 tests/openduet_input.py

# Actual firmware-only Recovery consumer; no obsolete native-PS/2 fixture.
EFI_HEADERS=${EFIINC:-${TOOLROOT:-}/usr/include/efi}
gcc $C -ffunction-sections -fdata-sections -Wl,--gc-sections -fshort-wchar -DGNU_EFI_USE_MS_ABI -I"$EFI_HEADERS" -I"$EFI_HEADERS/x86_64" tests/recovery_background.c -o "$out/recovery-background"
"$out/recovery-background"
gcc $C -fshort-wchar -DGNU_EFI_USE_MS_ABI -I"$EFI_HEADERS" -I"$EFI_HEADERS/x86_64" tests/recovery_firmware_pointer.c -o "$out/recovery-firmware-pointer"
"$out/recovery-firmware-pointer"
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

# Original synchronous control/descriptor pipeline, real URB/HCD and native EP0.
gcc $C -fno-pie -no-pie -pthread tests/linux_message.c compat/linux/usb_keyboard.c vendor/linux-v6.12/urb.c vendor/linux-v6.12/usbmouse.c -o "$out/linux-message";"$out/linux-message"

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

# Whole unchanged Linux hid-input + hid-generic units, host real I/O fault injection.
gcc $C -fno-builtin -fno-pie -no-pie tests/linux_hid_generic.c tests/linux_hid_runtime.c -o "$out/linux-hid-generic";"$out/linux-hid-generic"

# Actual generic/native adapter and shared input ownership, with scripted EP0.
gcc $C tests/linux_usb_generic.c $LX -o "$out/linux-usb-generic";"$out/linux-usb-generic"

# Actual original USB2 hub debounce/reset and shared CF exclusion.
gcc $C -no-pie -pthread tests/linux_hub.c -o "$out/linux-hub";"$out/linux-hub"

# Whole original Linux DMA pool and actual native EHCI descriptor consumers.
# Its private ABI include path must precede the older USB/HID compatibility ABI.
gcc -Icompat/linux/dma-include $C -Wno-unused-function -fno-pie -no-pie -pthread tests/linux_dma_pool.c -o "$out/linux-dma-pool";"$out/linux-dma-pool"
gcc -Icompat/linux/dma-include $C -Wno-unused-function -fwrapv -fno-sanitize=alignment -fno-pie -no-pie -c compat/linux/dma_pool.c -o "$out/dma-pool.o"
gcc -Icompat/linux/ehci-include -Icompat/linux/dma-include $C -fwrapv -fno-sanitize=alignment -Wno-unused-function -Wno-unused-but-set-variable -fno-pie -no-pie tests/linux_controller_dma.c compat/linux/ehci_pci.c "$out/dma-pool.o" -o "$out/linux-controller-dma";"$out/linux-controller-dma"
gcc -Icompat/linux/dma-include $C -fno-pie -no-pie tests/linux_dma_native.c -o "$out/linux-native-dma";"$out/linux-native-dma"

# Byte-exact whole UHCI on a nonidentity DMA hardware model. Alignment UBSan
# is disabled only for upstream list_entry sentinel pseudo-objects; actual
# DMA object alignment is checked separately, ASan/other UBSan stay enabled.
gcc -Icompat/linux/uhci-include -Icompat/linux/dma-include $C -fwrapv -fno-sanitize=alignment -Wno-unused-function -Wno-unused-but-set-variable -fno-pie -no-pie tests/linux_uhci.c -o "$out/linux-uhci";"$out/linux-uhci"

# Actual XML property-list serializer/parser, no fake .plist blobs.
gcc $C -fno-builtin -Iinclude tests/prefs_plist.c -o "$out/prefs-plist";"$out/prefs-plist"
