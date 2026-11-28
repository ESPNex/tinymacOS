#!/bin/bash
# Canonical USB core/hub/HID-core/usbhid/input/generic with original HCDs.
set -euo pipefail
D=$1;shift
mkdir -p "$D/usbcore"
A=compat/linux/usbcore-include
C=("$@" -std=gnu11 -fno-strict-aliasing -fwrapv -fno-pie -ffunction-sections -fdata-sections
 -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable
 -Wno-unused-but-set-variable -Wno-pointer-sign -Wno-maybe-uninitialized
 -Wno-int-in-bool-context -I"$A" -include "$A/tmc_kernel.h")
units=(vendor/linux-v6.12/usbcore-{usb,driver,generic,quirks,endpoint,buffer,port}.c
 vendor/linux-v6.12/{config,urb,hub,message,hcd,hid-input,hid-generic,hid-quirks}.c
 compat/linux/usbcore_{storage,services,platform,bridge,input,keymap,format,common,stack,hidcore,usbhid,hidservices}.c)
C+=(-Ivendor/linux-v6.12)

objects=()
for unit in "${units[@]}";do
 name=$(basename "$unit" .c)
 gcc "${C[@]}" -c "$unit" -o "$D/usbcore/$name.o"
 objects+=("$D/usbcore/$name.o")
done
# Native bootstrap registers the genuine bus/hub/generic/class drivers. Unused
# Linux USBFS/VFS/module-init paths are eliminated, NOT successful no-op stubs.
ld -r --gc-sections -u tmc_usb_stack_init -u tmc_usb_stack_poll -u tmc_usb_stack_leds -u vsnprintf -u tmc_usb_stack_storage_eject -u tmc_usb_stack_storage_ejectable -u tmc_usb_stack_storage_generation -u tmc_usb_stack_storage_count -u tmc_usb_stack_storage_size -u tmc_usb_stack_storage_bytes -u tmc_usb_stack_storage_model -u tmc_usb_stack_storage_io -u tmc_usb_stack_storage_flush "${objects[@]}" -o "$D/usbcore/combined.o"
# ld -r keeps orphan undefined SYMTAB entries after section GC. Remove them
# so no absent optional API can bind by name to a different private ABI.
objcopy --strip-unneeded "$D/usbcore/combined.o"
nm -g --defined-only "$D/usbcore/combined.o" | awk '$3 !~ /^tmc_usb_stack_/ && $3 !~ /^tmc_usb_physical_poll$/ {print $3, "tmc_usbcore_"$3}' > "$D/usbcore/prefix.map"
# Namespace every defined service to prevent cross-linking old private ABIs.
# Kernel hooks and typed/opaque original-controller APIs remain unchanged.
objcopy --redefine-syms="$D/usbcore/prefix.map" "$D/usbcore/combined.o" "$D/k-linux-usbcore.o"

# No accidental private-kernel or cross-ABI dependencies are permitted.
if nm -u "$D/k-linux-usbcore.o" | awk '{print $2}' | grep -Ev '^(memcmp|memcpy|memmove|memset|strlen|strncmp|tmc_linux_(xhci|ehci)_[a-z0-9_]+|tmc_usbcore_os_[a-z0-9_]+)$';then
 echo 'Unexpected cross-ABI dependency in canonical USB stack' >&2;exit 1
fi
