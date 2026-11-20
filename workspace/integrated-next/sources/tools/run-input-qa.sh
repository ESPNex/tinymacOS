#!/bin/bash
# QA-only native input topologies. External files only; never physical disks.
set -euo pipefail
cd "$(dirname "$0")/..";source tools/env.sh
: "${STATE:?External QA state required}"
[[ "$STATE" == /var/tmp/* ]] || exit 2
mkdir -p "$STATE"
Q=(-machine q35 -accel "${ACCEL:-tcg}" -cpu max -smp "${TMC_SMP:-2}" -m "${TMC_MEM:-768}" -vga std -L "$TOOLROOT/usr/share/qemu" -L "$TOOLROOT/usr/share/seabios")
case ${TMC_TEST_FIRMWARE:-uefi} in
 uefi)
 [[ -f "$STATE/vars.fd" ]]||cp "$TOOLROOT/usr/share/OVMF/OVMF_VARS_4M.fd" "$STATE/vars.fd"
 Q+=(-drive "if=pflash,format=raw,readonly=on,file=$TOOLROOT/usr/share/OVMF/OVMF_CODE_4M.fd" -drive "if=pflash,format=raw,file=$STATE/vars.fd");;
 bios) ;;
 *) exit 2;;
esac
case ${TMC_TEST_INPUT:-ps2} in
 ps2) ;;
 ehci|ehci-kbd)
 Q+=(-device ich9-usb-ehci1,id=ehci -device usb-mouse,bus=ehci.0,port=1,id=pointer,usb_version=2)
 if [[ "$TMC_TEST_INPUT" == ehci-kbd ]];then Q+=(-device usb-kbd,bus=ehci.0,port=2,usb_version=2);fi;;
 uhci) Q+=(-device piix3-usb-uhci,id=uhci -device usb-mouse,bus=uhci.0,port=1,id=pointer,usb_version=1);;
 companion|companion-kbd)
 Q+=(-device ich9-usb-ehci1,id=ehci -device ich9-usb-uhci1,masterbus=ehci.0,firstport=0,id=uhci -device usb-mouse,bus=ehci.0,port=1,id=pointer,usb_version=1)
 if [[ "$TMC_TEST_INPUT" == companion-kbd ]];then Q+=(-device usb-kbd,bus=ehci.0,port=2,usb_version=1);fi;;
 xhci) Q+=(-device qemu-xhci,id=xhci -device usb-mouse,bus=xhci.0,port=1,id=pointer);;
 *) exit 2;;
esac
if [[ ${BOOT_DISK:-0} != 1 ]];then
 [[ "${ISO:?}" == /var/tmp/* && -f "$ISO" ]] || exit 2
 Q+=(-drive "if=none,id=cd,media=cdrom,format=raw,readonly=on,file=$ISO" -device ide-cd,drive=cd,bus=ide.1,bootindex=1)
fi
if [[ ${LIVE:-0} != 1 ]];then
 disk=${DISK:-$STATE/system.raw};[[ "$disk" == /var/tmp/* ]]||exit 2
 if [[ ! -f "$disk" ]];then [[ ${BOOT_DISK:-0} != 1 ]]||exit 2;python3 tools/mkdisk.py "$disk";fi
 Q+=(-drive "if=none,id=hd,format=raw,file=$disk" -device ide-hd,drive=hd,bus=ide.0,bootindex=2)
fi
Q+=(-object rng-random,filename=/dev/urandom,id=rng -device virtio-rng-pci,rng=rng)
exec qemu-system-x86_64 "${Q[@]}" "$@"
