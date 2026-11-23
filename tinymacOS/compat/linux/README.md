# Linux USB compatibility — implementation status

The kernel builds and links **unmodified Linux v6.12 `drivers/hid/usbhid/usbmouse.c`**. `vendor/linux-v6.12/manifest.json` pins its source and license. The port is GPL-2.0-or-later; upstream notices remain intact.

Implemented runtime services are allocation/free with failure handling, coherent one-page buffers, logical interrupt URB submission/completion/synchronous cancellation, input registration/lifetime and key/relative/SYN delivery. The POSIX string/format subset has bounded writes and normal return/termination rules. Native kernel imports provide IRQ-safe serialization, heap/pages and real input events. A user-space POSIX libc alone cannot supply kernel DMA, IRQ or URB services.

Native xHCI/EHCI/UHCI enumeration feeds actual received interrupt packets into the Linux driver. Standard Boot mouse layouts use the upstream IRQ routine. Numbered reports, wide axes, nonstandard wheel layouts and absolute tablets retain the descriptor-aware native path; they are **not** re-labelled as Linux HID support. Allocation failure safely retains the native path.

**Not yet ported:** Linux HCDs, general USB core/hubs, generic HID and keyboards. This is not a replacement Linux USB stack and is not release completion. Recovery stays separate: firmware mouse protocols, OpenUsbKbDxe keymap and the declared derived firmware PS/2 raw stream.

Tests: `tests/linux_usbmouse.c` exercises the upstream driver, real compatibility code, failure cleanup, URB lifetime and concurrent lifecycles under ASan/UBSan. Existing native HCD host contracts link that same code. Guest receipts under `workspace/release-10.1.7-30B8/linux-usbmouse-*.json` independently verify the actual ISO kernel, live driver counters and rendered cursor positions. QEMU success is not physical hardware validation.

The next port milestone adds the complete, byte-exact Linux v6.12 EHCI `qtd_fill` function to the actual native EHCI DMA path. This covers descriptor page slicing, packet-safe truncation and upper DMA address words, with validation before DMA publication. It is not the remaining Linux EHCI scheduler, xHCI HCD, USB core or generic HID. The pinned full reference unit and exact extraction are source-checked on every build.
