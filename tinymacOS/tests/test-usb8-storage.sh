#!/bin/bash
# Tests actual BOT/SCSI class with explicit wire model, not a real USB disk.
set -euo pipefail
cd "$(dirname "$0")/.."
D=$(mktemp -d /var/tmp/tmc-usb8-storage-XXXXXXXX)
trap 'rm -rf "$D"' EXIT
for opt in -O1 -O2;do
 gcc -std=gnu11 "$opt" -g -ffunction-sections -fdata-sections -fno-strict-aliasing -fwrapv -fsanitize=address,undefined -fno-pie -no-pie -Icompat/linux/usbcore-include -Iinclude -include compat/linux/usbcore-include/tmc_kernel.h tests/usb8_storage_host.c -Wl,--gc-sections -o "$D/test"
 ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$D/test"
done
