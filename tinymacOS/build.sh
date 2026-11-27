#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"; ROOT=$PWD; source tools/env.sh
case "$TMC_BUILD" in /*) ;; *) echo 'TMC_BUILD must be an absolute external directory.' >&2; exit 2;; esac
# The production 10.2.5 installer MUST use FS2; never silently build legacy FS.
export TMC_FS2_BACKEND=1
# Match the verified published 10.2.4 USB/HID profile, not the shell default.
# Both live and Recovery kernels must select the complete canonical stack.
export TMC_USB_CANONICAL=1
D=$(realpath -m "$TMC_BUILD"); case "$D/" in "$ROOT/"*) echo 'Use a build directory outside the source tree.' >&2; exit 2;; esac
python3 tools/media_path.py "$D" "$D/$TMC_ISO"
mkdir -p "$D"; export TMC_BUILD="$D"; export LC_ALL=C
python3 tools/linux-source-contract.py
python3 tools/check-usb-retirement.py
C=(-O2 ${TMC_FS2_BACKEND:+-DTMC_FS2_BACKEND} ${TMC_DEBUG:+-DTMC_DEBUG} -g0 -ffreestanding -fno-stack-protector -fno-builtin -fno-asynchronous-unwind-tables -fno-unwind-tables -mno-red-zone -mgeneral-regs-only -fno-ident -ffile-prefix-map="$ROOT"=. -Wall -Wextra -Wno-misleading-indentation -Wno-sign-compare -Iinclude -Iarch)
C+=(-DTMC_USB_CANONICAL) # compatibility constant, no alternate USB build exists
U=("${C[@]}" -fpie -fno-plt -Iuserspace -Idesktop);L=(-nostdlib --build-id=none -z noexecstack -z max-page-size=0x1000)
E=("${C[@]}" -fPIC -Dmemcpy=tmc_efi_copy -Dmemset=tmc_efi_set -fshort-wchar -DGNU_EFI_USE_MS_ABI -I"$EFIINC" -I"$EFIINC/x86_64")
for f in crypto/sha256.c kernel/string.c storage/maintenance.c storage/partition.c;do gcc "${E[@]}" -c "$f" -o "$D/efi-${f##*/}.o";done
efilink(){ local name=$1;shift;ld -nostdlib --build-id=none -znocombreloc -T "$EFILIB/elf_x86_64_efi.lds" -shared -Bsymbolic "$EFILIB/crt0-efi-x86_64.o" "$@" "$D/efi-sha256.c.o" "$D/efi-string.c.o" -L"$EFILIB" -lgnuefi -o "$D/$name.so";objcopy --strip-all -j .text -j .rodata -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc --target=efi-app-x86_64 "$D/$name.so" "$D/$name.EFI";}
python3 tools/tramontana-assets.py "$D"
python3 tools/control-svg.py "$D"
python3 tools/settings-svg.py "$D"
U+=(-DTMC_ICON_ATLAS=\""$D/icons.atlas"\" -DTMC_CONTROL_SVG=\""$D/control-svg.alpha"\")
U+=(-DTMC_SETTINGS_SVG=\""$D/settings-svg.alpha"\")
for f in userspace/start.S kernel/string.c userspace/ui.c crypto/sha256.c; do gcc "${U[@]}" -c "$f" -o "$D/u-${f##*/}.o"; done
APPS=(desktop finder terminal textedit settings about calculator calendar monitor gallery diskutility notes stickies timemachine installer command)
for a in "${APPS[@]}"; do
 src=userspace/$a.c; units=(); objects=(); case "$a" in
 desktop) src=desktop/main.c;units=(desktop/gfx.c desktop/login.c desktop/glass.c desktop/control.c desktop/shell.c desktop/motion.c desktop/recovery.c desktop/assets.S);;
 finder) objects=(u-ui.c.o u-sha256.c.o);units=(userspace/preview.c userspace/inspector.c userspace/filebrowser.c userspace/fileops.c);;
 terminal) objects=(u-ui.c.o u-sha256.c.o);;
 command) objects=(u-ui.c.o u-sha256.c.o);units=(userspace/fileops.c);;
 textedit) objects=(u-ui.c.o);units=(userspace/filepanel.c userspace/filebrowser.c);;
 settings) objects=(u-ui.c.o);units=(userspace/fontmetrics.c userspace/vaultpanel.c);;
 about) objects=(u-ui.c.o);units=(userspace/fontmetrics.c);;
 diskutility) objects=(u-ui.c.o);units=(userspace/fontmetrics.c);;
 *) objects=(u-ui.c.o);; esac
 for f in "${units[@]}";do o="u-${f##*/}.o";gcc "${U[@]}" -c "$f" -o "$D/$o";objects+=("$o");done
 gcc "${U[@]}" -c "$src" -o "$D/u-$a.o"
 (cd "$D" && ld "${L[@]}" -T "$ROOT/userspace/layout.ld" u-start.S.o u-string.c.o "u-$a.o" "${objects[@]}" -o "$a.debug" && objcopy --strip-all "$a.debug" "$a.elf")
done
python3 tools/bios-image.py "$D"
C+=(-DTMC_COEXIST_PATH=\""$D/bios-coexist.bin"\")
C+=(-DTMC_MBR_PATH=\""$D/bios-mbr.bin"\")
python3 tools/build-ntfs.py "$D"
KOBJ=(k-native-ntfs.o)
# lwIP (IPv4 core) + in-kernel network glue
LWIP=(net/lwip/core/def.c net/lwip/core/inet_chksum.c net/lwip/core/init.c net/lwip/core/ip.c
      net/lwip/core/mem.c net/lwip/core/memp.c net/lwip/core/netif.c net/lwip/core/pbuf.c
      net/lwip/core/raw.c net/lwip/core/stats.c net/lwip/core/sys.c net/lwip/core/timeouts.c
      net/lwip/core/udp.c net/lwip/core/ipv4/dhcp.c net/lwip/core/ipv4/etharp.c
      net/lwip/core/ipv4/icmp.c net/lwip/core/ipv4/igmp.c net/lwip/core/ipv4/ip4.c
      net/lwip/core/ipv4/ip4_addr.c net/lwip/core/ipv4/ip4_frag.c net/lwip/netif/ethernet.c
      net/lwip/core/tcp.c net/lwip/core/tcp_in.c net/lwip/core/tcp_out.c)
for f in arch/*.S arch/*.c kernel/*.c drivers/*.c fs/*.c storage/*.c crypto/*.c crypto/monocypher/monocypher.c "${LWIP[@]}";do o="k-${f//\//-}.o";gcc "${C[@]}" -fno-pie -Inet/include -Inet/lwip/include -c "$f" -o "$D/$o";KOBJ+=("$o");done
# Sole canonical USB core/HID with shared original XHCI/EHCI transports.
# Whole original Linux DMA pool allocator uses its own private kernel ABI.
gcc "${C[@]}" -fno-pie -Icompat/linux/dma-include -Wno-unused-parameter -c compat/linux/dma_pool.c -o "$D/k-linux-dma-pool.o"
KOBJ+=(k-linux-dma-pool.o)
for f in compat/linux/ehci_runtime.c compat/linux/ehci_pci.c;do
 o="k-${f//\//-}.o";gcc "${C[@]}" -fwrapv -fno-pie -Icompat/linux/ehci-include -Wno-unused-parameter -Wno-unused-function -Wno-unused-but-set-variable -c "$f" -o "$D/$o";KOBJ+=("$o");done
for f in compat/linux/xhci_{runtime,pci,mem,ring,hub,extcaps,debug}.c;do
 o="k-${f//\//-}.o";gcc "${C[@]}" -fwrapv -fno-pie -Icompat/linux/xhci-include -Wno-unused-parameter -Wno-unused-function -Wno-unused-but-set-variable -c "$f" -o "$D/$o";KOBJ+=("$o");done

for f in compat/linux/ps2_services.c compat/linux/ps2_protocol.c compat/linux/ps2_input.c compat/linux/psmouse_input.c;do
 o="k-${f//\//-}.o";gcc "${C[@]}" -fno-pie -Icompat/linux/include -Wno-unused-parameter -c "$f" -o "$D/$o";KOBJ+=("$o");done
bash tools/build-usbcore.sh "$D" "${C[@]}";KOBJ+=(k-linux-usbcore.o)
# driver kernel extensions (ELF relocatables loaded from /System/Library/Extensions)
for k in e1000 wifi;do gcc "${C[@]}" -fno-pie -Iinclude -c "net/kext/$k.c" -o "$D/kext-$k.o";done
# Upstream CPU Jitter RNG must not be optimized: preserve timing work and enable all health tests.
for f in crypto/jitterentropy/*.c; do o="jent-${f##*/}.o"; gcc "${C[@]}" -O0 -fwrapv -fno-pie -Icrypto/jitterentropy -c "$f" -o "$D/$o"; KOBJ+=("$o"); done
(cd "$D" && ld "${L[@]}" -Map=kernel.map -T "$ROOT/kernel/layout.ld" "${KOBJ[@]}" -o kernel.debug && objcopy --strip-all kernel.debug kernel.elf)
# Separate native Recovery kernel: same proven native HCD/HID/storage code,
# distinct ELF and compile-time entry; never an EFI application.
gcc "${C[@]}" -DTMC_RECOVERY_KERNEL -fno-pie -Inet/include -Inet/lwip/include -c kernel/main.c -o "$D/recovery-entry.o"
REC_KOBJ=();for o in "${KOBJ[@]}";do if [[ "$o" == k-kernel-main.c.o ]];then REC_KOBJ+=(recovery-entry.o);else REC_KOBJ+=("$o");fi;done
(cd "$D" && ld "${L[@]}" -T "$ROOT/kernel/layout.ld" "${REC_KOBJ[@]}" -o recovery-kernel.debug && objcopy --strip-all recovery-kernel.debug RecoveryKernel.elf)
python3 tools/check-kernel-layout.py "$D/kernel.elf" "$D/RecoveryKernel.elf"
python3 tools/check-usb-profile.py "$D"
python3 tools/system_volume.py "$D" recovery
P=$(mktemp -d "$D/recovery.XXXXXXXX");trap 'rm -rf "$P"' EXIT
python3 tools/baseimage.py "$P" "$D/RecoveryKernel.elf" "$D/System-recovery.vol"
gcc "${E[@]}" -I"$P" -c boot/loader.c -o "$D/efi.o";efilink BOOTX64 "$D/efi.o"
gcc "${E[@]}" -DTMC_RECOVERY_ENTRY -I"$P" -c boot/loader.c -o "$D/rec-volume.o";efilink RECBOOT "$D/rec-volume.o"
gcc "${E[@]}" -c boot/firmware_tools.c -o "$D/firmware-video.o";efilink TmcVideo "$D/firmware-video.o"
gcc "${E[@]}" -DTMC_RESTORE_TOOL -c boot/firmware_tools.c -o "$D/firmware-restore.o";efilink TmcRestore "$D/firmware-restore.o"
# Browser intentionally omitted: legacy NetSurf retired; WebKit port not yet buildable.
python3 tools/system_volume.py "$D" cache;python3 tools/system_volume.py "$D" disk;python3 tools/system_volume.py "$D" live
bash tools/iso.sh "$P"
printf 'ISO (outside source): %s/%s\n' "$D" "$TMC_ISO"
