#!/bin/bash
# QA-only: the user's transport arrangement, PS/2 keyboard + direct USB pointer.
set -euo pipefail
cd "$(dirname "$0")/../../tinymacOS"; source tools/env.sh
: "${ISO:?}" "${STATE:?}"
[[ "$ISO" == /var/tmp/* && -f "$ISO" && "$STATE" == /var/tmp/* ]] || exit 2
mkdir -p "$STATE"
Q=(-machine q35 -accel "${ACCEL:-tcg}" -cpu max -smp 2 -m "${TMC_MEM:-512}" -vga std -L "$TOOLROOT/usr/share/qemu" -L "$TOOLROOT/usr/share/seabios")
case ${TMC_TEST_FIRMWARE:-uefi} in
 uefi)
 cp "$TOOLROOT/usr/share/OVMF/OVMF_VARS_4M.fd" "$STATE/vars.fd"
 Q+=(-drive "if=pflash,format=raw,readonly=on,file=$TOOLROOT/usr/share/OVMF/OVMF_CODE_4M.fd" -drive "if=pflash,format=raw,file=$STATE/vars.fd");;
 bios) ;;
 *) exit 2;;
esac
case ${TMC_POINTER:-mouse} in mouse) p=usb-mouse;; tablet) p=usb-tablet;; *) exit 2;;esac
case ${TMC_REC_POINTER_BUS:-xhci} in
 ehci) bus=ehci.0;extra=,usb_version=2;;
 xhci) bus=xhci.0;extra=;;
 *) exit 2;;
esac
Q+=(-device ich9-usb-ehci1,id=ehci -device qemu-xhci,id=xhci -device "$p,bus=$bus,port=1,id=pointer$extra")
Q+=(-drive "if=none,id=cd,media=cdrom,format=raw,readonly=on,file=$ISO" -device ide-cd,drive=cd,bus=ide.1,bootindex=1)
exec qemu-system-x86_64 "${Q[@]}" "$@"
