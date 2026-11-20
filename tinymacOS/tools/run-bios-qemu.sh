#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/..";source tools/env.sh
: "${TMC_BIOS_DISK:?Set TMC_BIOS_DISK to the disposable build-legacy.py output}"
[[ -f "$TMC_BIOS_DISK" && "$TMC_BIOS_DISK" == /var/tmp/* ]] || exit 2
exec qemu-system-x86_64 -machine q35 -accel "${ACCEL:-tcg}" -cpu max -smp "${TMC_SMP:-2}" -m 1024 -vga std \
 -L "$TOOLROOT/usr/share/qemu" -L "$TOOLROOT/usr/share/seabios" \
 -device ich9-usb-ehci1,id=ehci -device usb-kbd,bus=ehci.0,port=1 \
 -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0,port=1 -device usb-mouse,bus=xhci.0,port=2,id=pointer \
 -drive "if=none,id=bootdisk,format=raw,snapshot=on,file=$TMC_BIOS_DISK" -device ide-hd,drive=bootdisk,bus=ide.0,bootindex=1 \
 -object rng-random,filename=/dev/urandom,id=rng -device virtio-rng-pci,rng=rng "$@"
