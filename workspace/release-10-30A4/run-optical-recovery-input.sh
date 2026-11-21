#!/bin/bash
# One actual USB keyboard at a time: no ambiguous QEMU keyboard handler routing.
# Exercise EHCI and xHCI keyboard firmware paths in separate cold boots.
set -euo pipefail
cd "$(dirname "$0")/../../tinymacOS"; source tools/env.sh
: "${TMC_BIOS_ISO:?Actual optical ISO required}"
[[ -f "$TMC_BIOS_ISO" && "$TMC_BIOS_ISO" == /var/tmp/* ]] || exit 2
case ${TMC_POINTER:-mouse} in mouse) pointer=usb-mouse;; tablet) pointer=usb-tablet;; *) exit 2;; esac
case ${TMC_FIRMWARE_KBD_BUS:-xhci} in xhci) kb=xhci.0;; ehci) kb=ehci.0;; *) exit 2;; esac
exec qemu-system-x86_64 -machine q35,i8042=off -accel "${ACCEL:-tcg}" -cpu max -smp "${TMC_SMP:-2}" -m "${TMC_MEM:-512}" -vga std \
 -L "$TOOLROOT/usr/share/qemu" -L "$TOOLROOT/usr/share/seabios" \
 -device ich9-usb-ehci1,id=ehci -device qemu-xhci,id=xhci \
 -device "usb-kbd,bus=$kb,port=1,id=keyboard" -device "$pointer,bus=xhci.0,port=2,id=pointer" \
 -drive "if=none,media=cdrom,id=cd,format=raw,readonly=on,file=$TMC_BIOS_ISO" -device ide-cd,drive=cd,bus=ide.1,bootindex=1 \
 -object rng-random,filename=/dev/urandom,id=rng -device virtio-rng-pci,rng=rng "$@"
