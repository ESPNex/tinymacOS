#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-xhci-host.XXXXXXXX)
trap 'rm -rf "$out"' EXIT
# Alignment exception: original list_entry sentinel pseudo-objects, as with
# EHCI/UHCI. Hardware DMA alignment and nonidentity bus mapping are asserted.
# Exact upstream debug-only format uses Linux u64's %llx vs this OS's same-width
# unsigned long ABI. No format string or original source is changed.
C='-std=gnu11 -g -fwrapv -fsanitize=address,undefined -fno-sanitize=alignment -fno-omit-frame-pointer -fno-pie -no-pie -Wall -Wextra -Werror -Wno-format -Wno-maybe-uninitialized -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-misleading-indentation -Wno-sign-compare -Iinclude'
for opt in -O1 -O2;do
 gcc $C "$opt" -Icompat/linux/dma-include -c compat/linux/dma_pool.c -o "$out/dma.o"
 gcc $C "$opt" -Icompat/linux/xhci-include tests/linux_xhci_hcd.c compat/linux/xhci_pci.c compat/linux/xhci_mem.c compat/linux/xhci_ring.c compat/linux/xhci_hub.c compat/linux/xhci_extcaps.c compat/linux/xhci_debug.c "$out/dma.o" -o "$out/xhci"
 ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$out/xhci"
done
