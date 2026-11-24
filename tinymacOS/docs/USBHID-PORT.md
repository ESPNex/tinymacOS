# 10.1.7 / 30B8 — canonical USB/HID candidate

The canonical build connects the complete original Linux v6.12 xHCI/EHCI controller units, USB core/HCD/hub, HID-core, usbhid, hid-input, hid-generic and HID quirks. The canonical USB object no longer compiles or registers the Boot keyboard/mouse class drivers. The historical native class facade remains in the comparison build but does not own canonical devices. PS/2 is excluded.

## Build and reproduce

From `tinymacOS/`, with the dependencies in the system README:

```sh
export TOOLROOT=/var/tmp/tmc/tools/root
export TMC_BUILD=/var/tmp/tmc-usbhid/build
TMC_USB_CANONICAL=1 bash build.sh
python3 tools/dualboot-iso.py \
  "$TMC_BUILD/tinymacOS-10.1.7-x86_64.iso" \
  /var/tmp/tmc-usbhid/tinymacOS-10.1.7-usbhid.iso
bash tests/test-linux-usbcore.sh
bash tests/test-linux-usbcore-enumeration.sh
bash tests/test-linux-usbhid.sh
TMC_HID_OPT=-O2 bash tests/test-linux-usbhid.sh
bash tests/test-linux-xhci.sh
TMC_CANONICAL_STAGE=/var/tmp/tmc-usbhid \
TMC_CANONICAL_ISO=/var/tmp/tmc-usbhid/tinymacOS-10.1.7-usbhid.iso \
  python3 tests/linux_usbcore_guest.py uefi xhci-tablet-kbd
```

Repeat the last command for `bios` and `uefi`, and each of `xhci-two-mice-kbd`, `ehci-kbd`, `xhci-hub-kbd`, `xhci-tablet-kbd`. The guest verifies the kernel extracted from the actual ISO, live rendered input, old-path inactivity, worker progress, armed-URB disconnect and same-port re-enumeration. Absolute-pointer tests check the actual cursor sprite, not just counters.

## Implemented boundaries

- The canonical USB device model, USB bus/interface binding and HID bus share one ABI. Opaque controller handles and byte buffers isolate the original controller runtimes; no cross-ABI struct casts.
- A CPU0-pinned 64 KiB-stack kernel task owns polling/workqueue dispatch. Hardware input IRQ does not execute sleeping USB work. Initial asynchronous discovery is serviced before presenting interactive login (bounded 1.5-second wait), preventing silent loss of the first typed account name.
- Real control, interrupt-IN and interrupt-OUT URBs, exact report lengths, separate IN/OUT endpoint state, DMA, original HCD completion/unlink and teardown. Endpoint output is not simulated through a successful no-op or mislabeled EP0 request.
- Original report parsing, key translation, modifiers, input registration/open/close, managed-resource groups, driver attributes and binary report-descriptor attributes. Linux key codes are translated at the OS event boundary. Absolute values are clamped and normalized, with rounded framebuffer conversion.
- The source contract checks the canonical USB manifest and the additional 12 upstream HID pins. Original source/license notices are retained; the corresponding source kit accompanies the candidate.

## Evidence and scope

The authoritative final-media hashes, guest results, sanitizer runs, installation, firmware Recovery, UI and kernel checks are in `workspace/release-10.1.7-30B8/usbhid-validation.json` (repository-root relative). Earlier `usbcore-*` receipts describe the preceding Boot-only experiment and are not rewritten or promoted to usbhid evidence.

This is an experimental **USB/HID candidate**, not a claim that every Linux USB API/device class or physical computer is supported. The stable 10.1.6 release is not replaced.

## Remaining platform limits

- Serialized polling, not a Linux kernel scheduler or general workqueue/SMP API. Contended canonical locks/semaphores fail closed; arbitrary recursive/concurrent Linux callers are unsupported.
- No bulk/SG/iso transfer bridge, USB mass-storage/network/audio stack, hidraw/HIDDEV/BPF user API or general multitouch/gamepad application API.
- No physical IRQ routing, IOMMU/noncoherent-DMA or system-PM certification. Initialization/endpoint rollback and integrated reset/clear-halt require broader failure-injection coverage beyond existing controller models. The entropy ingestion hook credits no entropy.
- QEMU and explicit hardware models are separate evidence, neither physical ASRock/Bay Trail certification. Recovery uses OpenCore firmware protocols, not kernel Linux HID.
- The ISO is optical BIOS/UEFI, **not USB-DD hybrid**. For a physical live experiment follow [HARDWARE-TEST.md](HARDWARE-TEST.md): disconnect data disks and do not run the installer. Installation evidence concerns disposable virtual disks only.
