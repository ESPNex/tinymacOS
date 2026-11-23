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

## Historical v7: mouse button aggregation and controller error handoff

The Linux input adapter now merges held buttons across registered Linux-bound mice, while each report supplies only its own signed X/Y/wheel. Native xHCI/EHCI/UHCI error branches hand the failed logical interrupt URB back through real upstream core/usbmouse code; they no longer leave the affected mouse's held buttons stuck while native recovery is pending. Transient failures resubmit, terminal shutdown retires the URB. Native controller implementations are still not Linux HCDs; generic HID and the rest of USB core remain unported.

Host ASan/UBSan checks 1,024 paired five-button states, idle/release/unregister aggregation, transient STALL/EIO, all three terminal statuses, reopen, malformed status and leak-free teardown. The actual xHCI completion/reset path additionally verifies button release, native clear-halt/reset/rearm, resumed X/Y and terminal retirement with the real Linux mouse/URB attached. EHCI/UHCI error branches compile in the actual kernel; this is not a physical hardware fault-injection claim.

All twelve mouse and six keyboard profiles were rerun on v7, plus two UEFI/BIOS two-mouse profiles that check actual rendered Terminal-window movement across cross-device drag/release transitions. The first two-mouse harness attempt incorrectly used a flat font template on the glass titlebar; the next did not account for HMP's global button mask when switching the target mouse. Both failed receipts remain in `attempts/`. The corrected harness checks the pixel-exact rendered content rectangle and deliberately generates each selected device's button edge; no OS code was changed to accommodate the tests.

Full Linux controllers/core/generic-HID and the final install/kernel/Recovery release matrix remain outstanding. No deployment or ISO upload. The latest completed public release remains 10.1.6.

## Historical v8: genuine Linux PS/2 command/RX library; four-path scope explicit

Complete original Linux v6.12 `libps2.c` and header (GPL-2.0-only, hash-pinned) are now compiled through a separate compatibility wrapper. Kernel PS/2 keyboard setup and mouse wheel negotiation call original `ps2_command`; data and command responses pass original `ps2_interrupt`. Native i8042 I/O, Set-1 keyboard/PS2 mouse byte decoders and timer-safe LED machine remain native. This is not a complete `i8042`/`atkbd`/`psmouse` port. Blocking Linux PS/2 commands run during boot, not from the PIT handler; runtime LED ACKs use native nonblocking handling through Linux RX.

Host sanitizers cover ACK/NAK/resend, write failure, no-ACK mouse ID, keyboard ID, truncated response, partial reset, sliced command, bounded drain, real timeout, parity/timeout abort, independent RX locks/shared command mutex, interleaved input and callback-command rejection. UEFI+BIOS PS/2-only guests each execute nine real Linux setup commands, thirteen transmitted bytes, four-byte mouse wheel negotiation and zero command/RX errors. Actual Italian keyboard text/Caps/LEDs, six pixel-verified X/Y positions, and visible forward/back Terminal scrollback pass. The initial wheel harness used the opposite HMP direction; its failed receipts remain under `attempts/ps2-wheel-hmp-direction/`. Only the harness changed; OS code was unchanged for the passing rerun.

All twelve USB mouse, six USB keyboard and two mixed PS/2-keyboard/two-USB-mouse profiles were rerun on v8. The mixed cases observe original Linux PS/2 RX counters during enrollment, plus actual USB window dragging. Full Linux controllers, remaining USB core/hubs/HID, PS/2 class drivers and final release validation still block deployment. No physical Bay Trail result is claimed.

Workspace guard counts all files, reports both decimal MB and MiB, and enforces a conservative 120 MiB ceiling below the user's 128,000,000-byte limit. Private builds/ISOs and downloaded research remain under `/var/tmp`, not in snapshots; frozen 10.1.6 receipts remain untouched. Every verified source milestone is synchronized to GitHub before any future binary publication.

## Current v9: original psmouse packet/detect functions, shared PS/2+USB pointer ownership

Native PS/2 arithmetic/wheel decoder replaced with byte-exact complete original Linux packet/report functions. IntelliMouse/Explorer boot detection also executes original Linux functions using real libps2 AUX commands. Vendor source+header and both extraction ranges are hash-pinned; GPL-2.0-only provenance preserved. Native i8042 and stream framing, keyboard decoder/LEDs, remaining psmouse lifecycle/touchpad protocols and full Linux USB HCD/core/hub/generic HID remain incomplete.

Two PS/2-only ISO profiles pass with sixteen Linux commands, twenty-six transmitted bytes, Explorer four-byte negotiation, zero command/RX errors, Italian keyboard/Caps/LED, six exact rendered XY positions and bidirectional wheel. The twenty USB/two-USB-mouse profiles were rerun on this ISO, plus two new real PS/2+USB pointer profiles: **24 current profiles pass** across UEFI+BIOS. The latter verify actual Terminal window drag and button ownership survive both cross-transport release orders, not merely counters. Host ASan+UBSan runs at O1 and freestanding O2 check 3072 sign/axis combinations, full signed IMPS wheel, Explorer five buttons and wheel formats, stream recovery, 1024 PS/2+USB button states and failed negotiation rollback. Desktop horizontal scrolling is not implemented; the input path now delivers its independent horizontal component in EV_SCROLL.b.

The first v9 boot failed under optimized compilation. Its receipts are preserved under `attempts/psmouse-adapter-optimized-guard/`; the initial private adapter used a redundant embedded-device identity guard and its optimized probe fell through after the first command. The private adapter is now explicitly bound to the sole native AUX port instead of that identity assumption; both freestanding O1/O2 sanitizer runs and actual BIOS/UEFI boots pass. The first packaging attempt also ran out of external temporary disk space, without touching the workspace or published release; a byte-verified redundant private ISO was removed before retrying. Old source-synced v7/v8 private output directories were removed only after replacement guest checks passed. Source and evidence remain in Git history.

Independent normal build and dualboot packaging match byte-for-byte. Current ISO optical/installed/About/Recovery identity and actual OpenCore quiet/firmware input-driver media contracts pass. Final full install/kernel/Recovery release gates are still false; no deployment or physical Bay Trail claim.
