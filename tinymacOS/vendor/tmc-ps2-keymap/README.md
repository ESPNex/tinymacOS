# Declared OpenCore PS/2 firmware derivative

`TmcPs2KeyboardDxe.efi` is **not** an untouched official OpenCore artifact. It derives from the BSD-2-Clause-Patent Ps2KeyboardDxe firmware module that OpenCore ships. The original OpenCore binary and lock remain unchanged in `../opencore-1.0.8/`.

## Why

Firmware Unicode is layout-dependent and cannot reliably identify Italian physical punctuation. OpenCore KeySupport can also synthesize another stream from text input alongside OpenUsbKbDxe. Recovery therefore leaves **KeySupport disabled**, consumes OpenUsbKbDxe's physical Apple keymap for USB, and uses this driver's optional raw set-1 stream for PS/2. It does not access controller ports, include the native PS/2 driver, disconnect USB keyboard drivers, or issue HID requests.

The patch mirrors bytes the firmware driver already reads. Standard SimpleTextInput and SimpleTextInputEx remain installed and operational for the firmware picker. A bounded per-device queue reports overflow explicitly; Recovery clears prefix/modifier state rather than treating a truncated stream as intact. A distinctive binding version allows targeted firmware-to-firmware rebinding of the PNP0303 keyboard child only, never a host controller, mouse or storage handle.

## Corresponding source and build

- `upstream/` is byte-exact pinned source; `upstream-lock.json` records URLs, commit, archive hashes and build dependencies.
- `raw-set1.patch`, `TmcRawPs2.h`, `raw_ps2_queue.h` contain the changes. `build-lock.json` hashes source inputs and the actual shipped derived binary.
- Libraries/build platform use OpenCorePkg **1.0.8** and audk **0672a009e9ca85753d240324d761341adf0291b3**, not a guessed matching upstream binary.

Prepare an external audk tree with its pinned Brotli, libfdt and MipiSysT submodules and the OpenCorePkg 1.0.8 sources under `OpenCorePkg/`. GCC, Python 3, make and NASM are required. With the external tree initially pristine:

```sh
python3 prepare.py /external/UDK
python3 prepare_platform.py /external/UDK
cd /external/UDK
make -C BaseTools -j2
. ./edksetup.sh
build -p OpenCorePkg/TmcPs2.dsc -a X64 -b RELEASE -t GCC -n 2
```

The result is `Build/TmcPs2/RELEASE_GCC/X64/Ps2KeyboardDxe.efi`; the distributed copy has the distinct filename `TmcPs2KeyboardDxe.efi`. Build scripts verify its lock before staging the same driver set in all media paths. The license and notice are included in optical and installed media.

The ring and physical decoder have sanitizer tests in `../../tests/firmware_keyboard.c`. Four isolated guest regressions test PS/2 and USB under both firmware paths, including Italian punctuation, AltGr and UTF-8 backspace. QEMU success is **not** evidence of physical ASRock compatibility. This firmware work is separate from the still-incomplete Linux controller/core USB port after ExitBootServices.
