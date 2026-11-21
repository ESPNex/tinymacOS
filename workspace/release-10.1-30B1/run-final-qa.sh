#!/bin/bash
# One VM at a time: this workspace host has only 2 GiB of RAM.
set -euo pipefail
cd "$(dirname "$0")/../.."
export TOOLROOT=/var/tmp/tmc/tools/root OCR_ROOT=/var/tmp/tmc/tools/root TMC_MEM=512 TMC_BUILD=/var/tmp/tmc-10.1/build
export TMC_TEST_ISO=/var/tmp/tmc-10.1/tinymacOS-10.1-x86_64.iso
R=$PWD/workspace/release-10.1-30B1
python3 "$R/retry_kernel.py" > /var/tmp/tmc-10.1/kernel-retry.log 2>&1
echo 'PASS final kernel 29/29'
python3 "$R/control_qa.py" > /var/tmp/tmc-10.1/control.log 2>&1
echo 'PASS final Control Center'
python3 "$R/identity_qa.py" > /var/tmp/tmc-10.1/identity.log 2>&1
echo 'PASS final actual ISO identity and payload integrity'
python3 "$R/bios_installer_qa.py" > /var/tmp/tmc-10.1/install.log 2>&1
echo 'PASS final BIOS install, HDD reboot and native UEFI crossboot'
for fw in uefi bios;do
 for p in ps2 ehci xhci-hub-kbd;do
  export TMC_TEST_FIRMWARE=$fw TMC_TEST_INPUT=$p TMC_INPUT_RESULT="$R/input-$fw-$p.json"
  python3 tinymacOS/tests/input2_acceptance.py > /var/tmp/tmc-10.1/input-$fw-$p.log 2>&1
  echo "PASS final live $fw $p"
 done
done
python3 "$R/bios_recovery_qa.py" > /var/tmp/tmc-10.1/recovery.log 2>&1
echo 'PASS final offline Recovery sealed reinstall and document preservation'
for fw in uefi bios;do
 export TMC_TEST_FIRMWARE=$fw TMC_TEST_INPUT=ps2
 python3 "$R/recovery_keyboard_qa.py" > /var/tmp/tmc-10.1/recovery-kbd-$fw.log 2>&1
 echo "PASS final Recovery Italian keyboard $fw"
done
