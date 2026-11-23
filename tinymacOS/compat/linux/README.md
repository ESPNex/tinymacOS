# Linux USB/input compatibility — implemented boundaries

The requested complete Linux controller/core/hub port is **not finished**. PS/2 class drivers are deferred. Recovery remains a separate OpenCore firmware input path. QEMU and host tests do not certify physical Bay Trail hardware.

## Original Linux v6.12 code actually active

- Whole `usbmouse.c`/`usbkbd.c`, core `urb.c`/`config.c`, `hid-generic.c`/`hid-input.c` and `libps2.c`.
- Complete original EHCI `qtd_fill`, psmouse packet/report and IntelliMouse/Explorer detector functions, and HID descriptor/event/output function groups.
- Complete original `hcd.c` endpoint link/check/unlink, submission, unlink/refcount and direct-giveback functions now replace the manual lifecycle implementation.
- Complete original `hub.c` USB2 port debounce, reset-wait, warm-reset check and reset functions now run during native xHCI/EHCI hub-child enumeration. Actual EP0 bytes, checked short transfers, elapsed delays and CONFIGFLAG/reset reader/writer exclusion are provided by the native adapter.

`vendor/linux-v6.12/manifest.json` pins full upstream source hashes and verifies every function extraction byte-for-byte before build. Original notices remain intact. USB Boot/shim/HCD/hub code is GPL-2.0-or-later; core URB/config code is GPL-2.0-only; the input event header retains Linux-syscall-note.

## Native services and limits

The physical xHCI/EHCI/UHCI controllers still own DMA, command/transfer rings, scheduling, EP0 and child enumeration. A copying transport beneath original URB/HCD lifecycle functions supplies Boot interrupt-IN and keyboard EP0-OUT, with one active slot each and eight-byte payloads. Unsupported root-hub/SG/bulk/isoc/IN-control operations fail explicitly. These transport objects are not registered physical Linux HCDs or Linux root hubs. Original HCD endpoint/state gates and callback/anchor/reference ordering remain active.

Generic HID uses the actual native endpoint/EP0 adapter and the whole original generic/input units, then registered shared native keyboard/pointer consumers. It is a separate private ABI, not an alias of Boot USB/input objects. Limits: 1024-byte descriptor, 64-byte packet, input-frame size checked against endpoint capacity, 32 bindings, 16 application inputs each, eight copied pending reports. It supports bounded relative/absolute pointers and keyboard applications; unsupported models fail attachment. Italian ISO/AltGr events and held keys/buttons merge with Boot/PS2 input. Deferred LED output, reentrant reports, errors, queue overflow and detach preserve ownership.

The descriptor core also exposes a separate opaque parser with the upstream eight-configuration cap and BOS support. Native enumeration currently chooses first configuration/alternate zero, with at most 1024 received bytes. Configuration/HID/Boot objects are deliberately not interchangeable.

Locks, allocation, references, timed predicate waits and callback I/O are real implemented services. Waits run in the current native polled runtime, not a general Linux sleepable scheduler. Optional usbmon/KCOV/LED-trigger/debug instrumentation is disabled; no hardware operation is replaced with a successful no-op. USB2 hub helpers reject SuperSpeed at the public boundary and do not implement full hub lifecycle/hotplug. The original hub timing constants, including the 2000ms debounce cap, are retained.

## Still missing

Complete physical Linux xHCI/EHCI/UHCI drivers; the rest of general USB/HID core, device/bus registration, Linux root hubs, message transport and hub hotplug/disconnect/power lifecycle; general SG/bulk/isoc/DMA services; Linux i8042/serio/atkbd/complete psmouse (last, per user order). Source integration is not a completed release or deployment.

## Verification

ASan/UBSan contracts execute original code and actual adapters: reference/anchor/cancellation races, endpoint state/queue gates, unlink-status precedence, callback resubmit/free, allocation rollback, Italian/mixed input, generic report/LED/reentry handling, USB2 bounce/reset/error/timeouts and CONFIGFLAG writer exclusion. Actual ISO guests compare KernelCache to the build, observe non-secret upstream/native counters and independently check rendered input. Current receipts and remaining gates live in `workspace/release-10.1.7-30B8/STATUS.json`; older receipts only certify their recorded hashes.
