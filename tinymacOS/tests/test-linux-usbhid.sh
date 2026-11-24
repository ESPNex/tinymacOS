#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
D=$(mktemp -d /var/tmp/tmc-usbhid-host.XXXXXX);trap 'rm -rf "$D"' EXIT
R=compat/linux
C=(-std=gnu11 ${TMC_HID_OPT:--O1} -g -fwrapv -fno-strict-aliasing -fno-builtin -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie --param=asan-globals=0 -Werror=implicit-function-declaration -I"$R/usbcore-include" -Iinclude -Ivendor/linux-v6.12 -include "$R/usbcore-include/tmc_kernel.h")
units=(vendor/linux-v6.12/usbcore-{usb,driver,generic,quirks,endpoint,buffer,port}.c vendor/linux-v6.12/{config,urb,hub,message,hcd,hid-input,hid-generic,hid-quirks}.c)
for n in services platform input keymap common hidcore usbhid hidservices;do units+=("$R/usbcore_$n.c");done
units+=(tests/linux_usbhid_enumeration.c)
objs=()
for u in "${units[@]}";do n=$(basename "$u" .c);gcc "${C[@]}" -c "$u" -o "$D/$n.o";objs+=("$D/$n.o");done
gcc ${TMC_HID_OPT:--O1} -g -fsanitize=address,undefined -fno-pie -Dtmc_usbcore_os_poll=tmc_usbcore_host_idle -c tests/linux_usbcore_host.c -o "$D/host.o"
gcc -no-pie -fsanitize=address,undefined -Wl,--gc-sections "${objs[@]}" "$D/host.o" -o "$D/integration"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$D/integration"
