#!/bin/bash
# Original physical HCD nonidentity-DMA hardware MODELS, not real USB media QA.
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-usb8-bulk-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
C=(-std=gnu11 -g -fwrapv -fsanitize=address,undefined -fno-sanitize=alignment -fno-omit-frame-pointer -fno-pie -no-pie -Wall -Wextra -Werror -Wno-format -Wno-maybe-uninitialized -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-misleading-indentation -Wno-sign-compare -Iinclude -DTMC_USB8_BULK_TEST)
# Same sentinel pseudo-object alignment exception as the pre-existing HCD tests;
# actual DMA alignment/nonidentity addresses remain asserted by the models.
for opt in -O1 -O2;do
 gcc "${C[@]}" "$opt" -Icompat/linux/dma-include -c compat/linux/dma_pool.c -o "$out/dma.o"
 gcc "${C[@]}" "$opt" -DTMC_USB8_XHCI -Icompat/linux/xhci-include tests/linux_xhci_hcd.c compat/linux/xhci_pci.c compat/linux/xhci_mem.c compat/linux/xhci_ring.c compat/linux/xhci_hub.c compat/linux/xhci_extcaps.c compat/linux/xhci_debug.c "$out/dma.o" -o "$out/xhci"
 ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$out/xhci"
 gcc "${C[@]}" "$opt" -Icompat/linux/ehci-include tests/linux_ehci_hcd.c compat/linux/ehci_pci.c "$out/dma.o" -o "$out/ehci"
 ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$out/ehci"
done
