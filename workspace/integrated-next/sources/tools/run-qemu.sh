#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/..";. tools/env.sh
# The new medium defaults to an absolute tablet; older, deliberately selected CDs
# and the existing 1.3/2.0 consoles keep their supported relative mouse.
default_pointer=tablet
selected_iso=${ISO:-};case ${selected_iso##*/} in tinymacOS-1.*.iso|tinymacOS-2.0-x86_64.iso) default_pointer=mouse;; esac
case ${TMC_POINTER:-$default_pointer} in tablet) pointing=usb-tablet;; mouse) pointing=usb-mouse;; *) echo 'TMC_POINTER: scegliere tablet o mouse.' >&2;exit 2;; esac
# Do not silently substitute an older external Recovery for the integrated CD.
if [[ -n ${RECOVERY_IMG:-} && ${selected_iso:-$TMC_ISO} == *tinymacOS-2.4.4-x86_64.iso ]];then
 echo 'Nella 2.4.4 BaseSystem è già incluso: rimuovi RECOVERY_IMG e premi R alla mela.' >&2;exit 2
fi
OVMF_DIR=${OVMF_DIR:-${TOOLROOT:-}/usr/share/OVMF};OVMF_CODE=${OVMF_CODE:-$OVMF_DIR/OVMF_CODE_4M.fd};OVMF_VARS=${OVMF_VARS:-$OVMF_DIR/OVMF_VARS_4M.fd};STATE=${STATE:-state};mkdir -p "$STATE";[[ -f "$STATE/vars.fd" ]]||cp "$OVMF_VARS" "$STATE/vars.fd"
# Optional reproducible multi-controller topology for input regression.
extra_usb=();kbd_bus=xhci.0;pointer_bus=xhci.0
case ${TMC_USB_LAYOUT:-standard} in
 standard) ;;
 second) extra_usb=(-device qemu-xhci,id=xhci2);kbd_bus=xhci2.0;pointer_bus=xhci2.0;;
 split) extra_usb=(-device qemu-xhci,id=xhci2);pointer_bus=xhci2.0;;
 *) echo 'TMC_USB_LAYOUT: standard, second oppure split' >&2;exit 2;;
esac
Q=(-machine q35,i8042=off -accel "${ACCEL:-tcg}" -cpu "${TMC_CPU:-max}" -smp "${TMC_SMP:-2}" -m "${TMC_MEM:-1024}" ${TMC_NET:+-netdev user,id=net0 -device e1000,netdev=net0} -vga std -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE" -drive "if=pflash,format=raw,file=$STATE/vars.fd" -device qemu-xhci,id=xhci "${extra_usb[@]}" -device "usb-kbd,bus=$kbd_bus,port=1" -device "$pointing,bus=$pointer_bus,port=2,id=pointer")
if [[ -n ${RECOVERY_IMG:-} ]];then [[ -f "$RECOVERY_IMG" ]]||{ echo 'Immagine di recupero non trovata.' >&2;exit 2;};Q+=(-drive "if=none,id=rec,format=raw,readonly=on,file=$RECOVERY_IMG" -device usb-storage,drive=rec,bus=xhci.0,port=3,bootindex=1);elif [ "${BOOT_DISK:-0}" != 1 ];then ISO=${ISO:-$TMC_BUILD/$TMC_ISO};[[ -f "$ISO" ]]||{ echo 'ISO mancante: eseguire build.sh.' >&2;exit 2;};Q+=(-drive "if=none,media=cdrom,id=cd,readonly=on,format=raw,file=$ISO" -device ide-cd,drive=cd,bus=ide.1,bootindex=1);fi
# Q35's ide.N bus names refer to the six ports on its ICH9 AHCI controller, not legacy IDE ports.
if [[ ${LIVE:-0} != 1 ]];then disk=${DISK:-$STATE/system.raw}; if [[ ! -e "$disk" ]];then [[ ${BOOT_DISK:-0} != 1 && -z ${DISK:-} ]] || { echo 'Disco SATA non trovato.' >&2;exit 2;};python3 tools/mkdisk.py "$disk";fi;[[ -f "$disk" ]]||exit 2;Q+=(-drive "if=none,id=hd,format=raw,file=$disk" -device ide-hd,drive=hd,bus=ide.0,bootindex=2);fi
if [[ ${LIVE:-0} != 1 && ${BOOT_DISK:-0} != 1 ]];then
 old=${LEGACY_DATA:-$STATE/user.img};if [[ -n ${LEGACY_DATA:-} && ! -f "$old" ]];then echo 'Volume dati precedente non trovato' >&2;exit 2;fi
 if [[ -f "$old" && ! "$old" -ef "$disk" ]];then Q+=(-drive "if=none,id=old,format=raw,snapshot=on,file=$old" -device ide-hd,drive=old,bus=ide.2);fi
fi
if [[ ${TMC_RNG:-host} == host ]];then Q+=(-object rng-random,filename=/dev/urandom,id=rng -device virtio-rng-pci,rng=rng);fi
if [[ -n ${TOOLROOT:-} ]];then Q+=(-L "$TOOLROOT/usr/share/qemu" -L "$TOOLROOT/usr/share/seabios");fi
exec qemu-system-x86_64 "${Q[@]}" "$@"
