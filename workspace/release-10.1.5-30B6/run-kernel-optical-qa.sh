#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
export TOOLROOT=/var/tmp/tmc/tools/root OCR_ROOT=/var/tmp/tmc/tools/root TMC_MEM=512 TMC_PRODUCTION_BUILD=/var/tmp/tmc-10.1.5/build
R=$PWD/workspace/release-10.1.5-30B6
python3 workspace/webkit-tmc/tools/run-tests.py --guest > /var/tmp/tmc-10.1.5/kernel.log 2>&1
cp workspace/webkit-tmc/test-results/milestone-tests.json "$R/kernel-regression-result.json"
for kind in vm network sync tls threads;do cp "workspace/webkit-tmc/test-results/guest-$kind-media.json" "$R/kernel-probe-$kind.json";done
for kind in vm network sync tls threads;do
 cpus="1 2";[[ "$kind" == sync || "$kind" == tls || "$kind" == threads ]] && cpus="1 4 8"
 for cpu in $cpus;do
 src="workspace/webkit-tmc/test-results/guest-$kind-smp$cpu.json"
 [[ "$kind" == vm || "$kind" == network ]] && src="workspace/webkit-tmc/test-results/guest-$kind-smp$cpu/result.json"
 cp "$src" "$R/kernel-guest-$kind-smp$cpu.json"
 done
done
export TMC_BUILD=/var/tmp/tmc-10.1.5/build TMC_TEST_ISO=/var/tmp/tmc-10.1.5/tinymacOS-10.1.5-x86_64.iso TMC_BIOS_ISO=/var/tmp/tmc-10.1.5/tinymacOS-10.1.5-x86_64.iso
for bus in ehci xhci;do
 for key in r f12;do
  export TMC_FIRMWARE_KBD_BUS=$bus TMC_REC_BOOT_KEY=$key TMC_OPTICAL_BIOS_LAUNCHER=$R/run-optical-recovery-input.sh TMC_POINTER_RESULT=$R/recovery-pointer-bios-$bus-$key.json
  python3 "$R/recovery_pointer_qa.py" --firmware bios /var/tmp/tmc-10.1.5/optical-$bus-$key > /var/tmp/tmc-10.1.5/optical-$bus-$key.log 2>&1
  echo "PASS optical BIOS $bus $key"
 done
done
unset TMC_OPTICAL_BIOS_LAUNCHER TMC_REC_BOOT_KEY
export TMC_POINTER_RESULT=$R/recovery-pointer-uefi.json
python3 "$R/recovery_pointer_qa.py" --firmware uefi /var/tmp/tmc-10.1.5/optical-uefi > /var/tmp/tmc-10.1.5/optical-uefi.log 2>&1
echo 'PASS optical UEFI'
bash "$R/run-extra-recovery.sh" > /var/tmp/tmc-10.1.5/extra-recovery-matrix.log 2>&1
echo 'PASS extra PS/2 keyboard + direct USB pointer matrix'
for fw in uefi bios;do
 for bus in ehci xhci;do
  TMC_TEST_FIRMWARE=$fw TMC_TEST_INPUT=$bus python3 "$R/native-sustained-qa.py" >> /var/tmp/tmc-10.1.5/native-sustained.log 2>&1
 done
done
echo 'PASS native sustained mouse BIOS/UEFI EHCI/xHCI'
