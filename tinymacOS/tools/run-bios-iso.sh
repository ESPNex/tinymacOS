#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/..";source tools/env.sh
: "${TMC_BIOS_ISO:?Set TMC_BIOS_ISO to the dual-boot optical ISO}"
[[ -f "$TMC_BIOS_ISO" && "$TMC_BIOS_ISO" == /var/tmp/* ]] || exit 2
case ${TMC_POINTER:-mouse} in mouse) pointer=usb-mouse;;tablet) pointer=usb-tablet;;*) exit 2;;esac
exec qemu-system-x86_64 -machine q35 -accel "${ACCEL:-tcg}" -cpu max -smp "${TMC_SMP:-2}" -m 1024 -vga std \
 -L "$TOOLROOT/usr/share/qemu" -L "$TOOLROOT/usr/share/seabios" \
 -device ich9-usb-ehci1,id=ehci -device usb-kbd,bus=ehci.0,port=1 \
 -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0,port=1 -device "$pointer,bus=xhci.0,port=2,id=pointer" \
 -drive "if=none,media=cdrom,id=cd,format=raw,readonly=on,file=$TMC_BIOS_ISO" -device ide-cd,drive=cd,bus=ide.1,bootindex=1 \
 -object rng-random,filename=/dev/urandom,id=rng -device virtio-rng-pci,rng=rng "$@"
