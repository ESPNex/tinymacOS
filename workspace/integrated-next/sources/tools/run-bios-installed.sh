#!/bin/bash
# QA-only launcher: newly-created disposable SATA file and the actual release ISO.
set -euo pipefail
cd "$(dirname "$0")/..";source tools/env.sh
: "${STATE:?Use an external QA state directory}"
[[ "$STATE" == /var/tmp/* ]] || { echo 'External QA state required' >&2;exit 2; }
mkdir -p "$STATE"
DISK=${DISK:-$STATE/system.raw}
[[ "$DISK" == /var/tmp/* ]] || exit 2
if [[ ! -f "$DISK" ]];then [[ ${BOOT_DISK:-0} == 0 ]] || exit 2;python3 tools/mkdisk.py "$DISK";fi
case ${TMC_POINTER:-mouse} in mouse) pointer=usb-mouse;;tablet) pointer=usb-tablet;;*) exit 2;;esac
Q=(-machine q35 -accel "${ACCEL:-tcg}" -cpu max -smp "${TMC_SMP:-2}" -m 1024 -vga std
 -L "$TOOLROOT/usr/share/qemu" -L "$TOOLROOT/usr/share/seabios"
 -device ich9-usb-ehci1,id=ehci -device usb-kbd,bus=ehci.0,port=1
 -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0,port=1 -device "$pointer,bus=xhci.0,port=2,id=pointer"
 -drive "if=none,id=hd,format=raw,file=$DISK" -device ide-hd,drive=hd,bus=ide.0,bootindex=2
 -object rng-random,filename=/dev/urandom,id=rng -device virtio-rng-pci,rng=rng)
if [[ ${BOOT_DISK:-0} == 0 ]];then
 : "${TMC_BIOS_ISO:?Dual-boot ISO required for installation}"
 [[ -f "$TMC_BIOS_ISO" ]] || exit 2
 Q+=(-drive "if=none,media=cdrom,id=cd,format=raw,readonly=on,file=$TMC_BIOS_ISO" -device ide-cd,drive=cd,bus=ide.1,bootindex=1)
fi
exec qemu-system-x86_64 "${Q[@]}" "$@"
