# 10.1.7 / 30B8 — development, not published

Current status is machine-readable in `STATUS.json`. Every guest receipt identifies its actual ISO; older successful tests do not certify a later binary.

## Implemented port milestone

Linux v6.12 `usbmouse.c` is preserved byte-for-byte upstream, compiled and linked into the native kernel. The implemented USB/input compatibility subset receives real interrupt packets from **native** xHCI/EHCI/UHCI transport. The historical v3 twelve BIOS/UEFI topologies verified live driver completions plus 72 exact rendered cursor positions; their ISO hashes do not certify v5. Host sanitizers cover allocation failure, cancellation/reopen, held-button release on disconnect and 4,000 concurrent lifecycles. This is a Boot mouse HID port, not Linux HCD/general USB-core/generic-HID support.

## Recovery

Firmware relative counts are converted using counts/mm, with per-device signed fractional state. The direct USB mouse transport is not linked into Recovery. Keyboard transport now uses OpenUsbKbDxe’s Apple keymap and the declared derived Ps2KeyboardDxe raw-set-1 protocol. No native keyboard transport, direct i8042 I/O or UsbIo keyboard requests are linked into Recovery.

The attempted Apple keymap/KeySupport combination duplicated USB input. A SimpleTextInputEx PS/2 adapter lost correct punctuation. Both implementations were reverted; their failed receipts remain under `attempts/`. The subsequent firmware implementation avoids both failures: KeySupport stays off, USB remains on OpenUsbKbDxe, and PS/2 uses physical bytes mirrored by the firmware driver rather than guessing positions from Unicode. Four current v3 guest profiles pass the exact Italian punctuation string, AltGr and UTF-8 backspace. The derivative is separately named and source-locked; original OpenCore binaries/lock stay untouched.

## Harness/resource failures retained

- A concurrent BIOS staging attempt lost QMP when the host OOM killer terminated QEMU. The OOM trace and failed receipt are retained; a memory-bounded rerun passed. Do not run more than two guests in this 2 GiB workspace.
- Control Center initially failed a flat-pixel text template on its glass backdrop although the panel was visibly open. The harness now uses cropped pixel/OCR verification of the actual labels, as for its other glass controls; no click is treated as proof.

Through v3, About source and executable were unchanged from 10.1.6 / 30B7 by the earlier request. The latest user instruction reverses that exception: v4 updates only the version/build strings to 10.1.7 / 30B8, retaining the layout. The icon manifest's historical 10.1.6 provenance note is likewise unchanged. No QEMU result is physical ASRock compatibility evidence. Final publication remains blocked on the remaining implementation and complete release gates.

## Historical v3 integration

ISO SHA-256 `cfed0081712277fd25b7020f56c15fe52e252316a42f5c1c64d116ca200fae03`, 225718272 bytes; two independent full builds are byte-identical. The linked KernelCache is byte-identical to v2 (the tested 29-case/13-profile kernel). Firmware/loader/Recovery changes still require their own current receipts; old v2 ISO-specific input receipts do not certify v3. Current v3 has four keyboard profiles, UEFI+BIOS staging/deletion/Recovery mouse and media/identity contracts. Linux HCD/core/generic HID and the complete final release gate remain unfinished; no deployment.

The subsequent v3 retest also passed all twelve Linux Boot mouse topologies (72 exact rendered XY checks, live upstream completion counters) and the complete Control Center guest checks. These receipts identify the v3 ISO, not v2. Kernel evidence is carried only through the exact-byte comparison recorded in `kernel-binary-equivalence-v3.json`; it is not reported as a newly run kernel suite.

## Historical v4 integration

Current v4 adds the full unmodified Linux EHCI `qtd_fill` function to native EHCI descriptor construction, not the full HCD. Host sanitizers cover 147672 page/DMA64/packet combinations plus rollback. UEFI+BIOS EHCI guests independently verify live qTD-build counters, upstream Boot mouse completions and rendered XY. Shared Intel mux validation now checks current routing registers before either write and confirms readback; six controller-family fixtures include Bay Trail 8086:0f35. These fixtures do not emulate Bay Trail electrical behavior or certify ASRock hardware. About displays 10.1.7 / 30B8 by the latest instruction. Kernel bytes changed, so the old v2/v3 exact-kernel equivalence must NOT be carried to v4. Remaining controller/core/generic-HID work and final release gates still block deployment.

## Historical v5: complete upstream Boot keyboard, not complete USB stack

The full byte-exact Linux v6.12 `usbkbd.c` is compiled through `compat/linux/usb_keyboard.c`. Native xHCI/EHCI/UHCI feed its interrupt URB. Its actual input events enter the sole Italian map; registered keyboards merge held-key bitmaps/modifiers without a six-key aggregate limit. The original upstream LED event/completion state machine now reaches native EP0 through a bounded deferred control adapter. Generation and submission-serial validation guard cancellation/disconnect/rebind while that native wait is outside the compatibility lock.

ASan/UBSan executes seven allocation rollback points, Italian Shift/AltGr/accent/euro/ISO keys, Caps/repeat, two-keyboard shared keys/modifiers and twelve held keys, invalid/rollover packets, interrupt cancel/reopen, LED coalescing/reentry, same-URB resubmit ABA, concurrent disconnect/rebind ABA, STALL and leak-free teardown. Six actual ISO guests pass: UEFI+BIOS on EHCI, UHCI companion and xHCI hub, with i8042 disabled. Each records 180 upstream IRQ completions/frames, two native LED controls and zero rejected/control-error/stale completions. This verifies virtual devices, not ASRock/Bay Trail hardware.

The v5 kernel differs from v4. Earlier full staging, recovery, media and runtime receipts must not be treated as current-binary certification. Linux HCDs, general USB core/hubs, generic HID and the complete final install/input/kernel/license gate still block deployment. Source synchronization only; no release or ISO upload.

## Historical v6: full upstream core/urb.c, still not a full Linux USB port

The entire byte-exact Linux v6.12 `drivers/usb/core/urb.c` now builds and supplies the actual URB API, replacing the handwritten allocation/free/submit/kill implementations. Source is GPL-2.0-only, separately identified in the source manifest. The compatibility layer supplies real reference/list/atomic/IRQ-lock/wait services; optional debug/sanitizer instrumentation is disabled as in non-debug upstream configurations. The backend remains a bounded native-HCD adapter (one interrupt-IN and EP0-OUT slot per binding, eight bytes maximum, no bulk/isoc/SG/IN-control), not Linux `hcd.c` or a full scheduler/core.

Host ASan/UBSan covers reference/size overflow, active-driver-put and callback-free lifetime, short packets, state/endpoint/packet/SG/control/interval checks, slot backpressure, kill/unlink/poison/block, every anchor API, real timeout and concurrent wakeup. Mouse/keyboard host and native maintenance/legacy suites pass. A missing `hid_linux_keys` declaration caused by the overlay's earlier reverse copy was restored in both canonical and overlay headers; the final builds compile and the overlay check is run read-only. Do not use `--apply` to copy canonical edits into the overlay: it copies in the opposite direction.

The final v6 ISO is independently reproducible and remains private. All twelve mouse plus six USB-only keyboard QEMU profiles were rerun on its exact KernelCache. Besides rendered input, each observes live upstream URB submissions/givebacks and the invariant `submits = givebacks + active`, with zero backend rejects. This is 18 virtual configurations, not physical ASRock/Bay Trail evidence. Older staging/Recovery/kernel receipts do not certify this changed kernel. Complete Linux controllers, remaining USB core/hubs, generic HID and final release tests still block completion and deployment.

## Current v7: mouse button aggregation and controller error handoff

The Linux input adapter now merges held buttons across registered Linux-bound mice, while each report supplies only its own signed X/Y/wheel. Native xHCI/EHCI/UHCI error branches hand the failed logical interrupt URB back through real upstream core/usbmouse code; they no longer leave the affected mouse's held buttons stuck while native recovery is pending. Transient failures resubmit, terminal shutdown retires the URB. Native controller implementations are still not Linux HCDs; generic HID and the rest of USB core remain unported.

Host ASan/UBSan checks 1,024 paired five-button states, idle/release/unregister aggregation, transient STALL/EIO, all three terminal statuses, reopen, malformed status and leak-free teardown. The actual xHCI completion/reset path additionally verifies button release, native clear-halt/reset/rearm, resumed X/Y and terminal retirement with the real Linux mouse/URB attached. EHCI/UHCI error branches compile in the actual kernel; this is not a physical hardware fault-injection claim.

All twelve mouse and six keyboard profiles were rerun on v7, plus two UEFI/BIOS two-mouse profiles that check actual rendered Terminal-window movement across cross-device drag/release transitions. The first two-mouse harness attempt incorrectly used a flat font template on the glass titlebar; the next did not account for HMP's global button mask when switching the target mouse. Both failed receipts remain in `attempts/`. The corrected harness checks the pixel-exact rendered content rectangle and deliberately generates each selected device's button edge; no OS code was changed to accommodate the tests.

Full Linux controllers/core/generic-HID and the final install/kernel/Recovery release matrix remain outstanding. No deployment or ISO upload. The latest completed public release remains 10.1.6.
