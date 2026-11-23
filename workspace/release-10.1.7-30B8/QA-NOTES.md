# 10.1.7 / 30B8 — development, not published

Current status is machine-readable in `STATUS.json`. Every guest receipt identifies its actual ISO; older successful tests do not certify a later binary.

## Implemented port milestone

Linux v6.12 `usbmouse.c` is preserved byte-for-byte upstream, compiled and linked into the native kernel. The implemented USB/input compatibility subset receives real interrupt packets from **native** xHCI/EHCI/UHCI transport. Twelve BIOS/UEFI topologies verify live driver completions plus 72 exact rendered cursor positions. Host sanitizers cover allocation failure, cancellation/reopen, held-button release on disconnect and 4,000 concurrent lifecycles. This is a Boot mouse HID port, not Linux HCD/general USB-core/generic-HID support.

## Recovery

Firmware relative counts are converted using counts/mm, with per-device signed fractional state. The direct USB mouse transport is not linked into Recovery. Keyboard transport now uses OpenUsbKbDxe’s Apple keymap and the declared derived Ps2KeyboardDxe raw-set-1 protocol. No native keyboard transport, direct i8042 I/O or UsbIo keyboard requests are linked into Recovery.

The attempted Apple keymap/KeySupport combination duplicated USB input. A SimpleTextInputEx PS/2 adapter lost correct punctuation. Both implementations were reverted; their failed receipts remain under `attempts/`. The subsequent firmware implementation avoids both failures: KeySupport stays off, USB remains on OpenUsbKbDxe, and PS/2 uses physical bytes mirrored by the firmware driver rather than guessing positions from Unicode. Four current v3 guest profiles pass the exact Italian punctuation string, AltGr and UTF-8 backspace. The derivative is separately named and source-locked; original OpenCore binaries/lock stay untouched.

## Harness/resource failures retained

- A concurrent BIOS staging attempt lost QMP when the host OOM killer terminated QEMU. The OOM trace and failed receipt are retained; a memory-bounded rerun passed. Do not run more than two guests in this 2 GiB workspace.
- Control Center initially failed a flat-pixel text template on its glass backdrop although the panel was visibly open. The harness now uses cropped pixel/OCR verification of the actual labels, as for its other glass controls; no click is treated as proof.

Through v3, About source and executable were unchanged from 10.1.6 / 30B7 by the earlier request. The latest user instruction reverses that exception: v4 updates only the version/build strings to 10.1.7 / 30B8, retaining the layout. The icon manifest's historical 10.1.6 provenance note is likewise unchanged. No QEMU result is physical ASRock compatibility evidence. Final publication remains blocked on the remaining implementation and complete release gates.

## Current v3 integration

ISO SHA-256 `cfed0081712277fd25b7020f56c15fe52e252316a42f5c1c64d116ca200fae03`, 225718272 bytes; two independent full builds are byte-identical. The linked KernelCache is byte-identical to v2 (the tested 29-case/13-profile kernel). Firmware/loader/Recovery changes still require their own current receipts; old v2 ISO-specific input receipts do not certify v3. Current v3 has four keyboard profiles, UEFI+BIOS staging/deletion/Recovery mouse and media/identity contracts. Linux HCD/core/generic HID and the complete final release gate remain unfinished; no deployment.

The subsequent v3 retest also passed all twelve Linux Boot mouse topologies (72 exact rendered XY checks, live upstream completion counters) and the complete Control Center guest checks. These receipts identify the v3 ISO, not v2. Kernel evidence is carried only through the exact-byte comparison recorded in `kernel-binary-equivalence-v3.json`; it is not reported as a newly run kernel suite.

## v4 work in progress

Current v4 adds the full unmodified Linux EHCI `qtd_fill` function to native EHCI descriptor construction, not the full HCD. Host sanitizers cover 147672 page/DMA64/packet combinations plus rollback. UEFI+BIOS EHCI guests independently verify live qTD-build counters, upstream Boot mouse completions and rendered XY. Shared Intel mux validation now checks current routing registers before either write and confirms readback; six controller-family fixtures include Bay Trail 8086:0f35. These fixtures do not emulate Bay Trail electrical behavior or certify ASRock hardware. About displays 10.1.7 / 30B8 by the latest instruction. Kernel bytes changed, so the old v2/v3 exact-kernel equivalence must NOT be carried to v4. Remaining controller/core/generic-HID work and final release gates still block deployment.
