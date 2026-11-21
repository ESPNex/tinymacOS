set -euo pipefail
export TOOLROOT=/var/tmp/tmc/tools/root OCR_ROOT=/var/tmp/tmc/tools/root TMC_MEM=512 TMC_BUILD=/var/tmp/tmc-10.1.4/build TMC_TEST_ISO=/var/tmp/tmc-10.1.4/tinymacOS-10.1.4-x86_64.iso TMC_BIOS_ISO=/var/tmp/tmc-10.1.4/tinymacOS-10.1.4-x86_64.iso
for fw in uefi bios;do
 export TMC_TEST_FIRMWARE=$fw
 for bus in ehci xhci;do
  export TMC_REC_POINTER_BUS=$bus TMC_POINTER_RESULT=$PWD/workspace/release-10.1.4-30B5/recovery-ps2-usb-$fw-$bus.json
  python3 workspace/release-10.1.4-30B5/recovery_ps2_usb_qa.py --firmware "$fw" /var/tmp/tmc-10.1.4/ps2-usb-$fw-$bus > /var/tmp/tmc-10.1.4/ps2-usb-$fw-$bus.log 2>&1
  echo "PASS PS/2 keyboard + direct USB mouse/tablet $fw $bus"
 done
done
