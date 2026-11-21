#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
export TOOLROOT=/var/tmp/tmc/tools/root OCR_ROOT=/var/tmp/tmc/tools/root TMC_MEM=512 TMC_BUILD=/var/tmp/tmc-10.1.4/build
export TMC_TEST_ISO=/var/tmp/tmc-10.1.4/tinymacOS-10.1.4-x86_64.iso
R=$PWD/workspace/release-10.1.4-30B5
python3 "$R/title_qa.py" > /var/tmp/tmc-10.1.4/title.log 2>&1;echo 'PASS actual title centering'
python3 "$R/control_qa.py" > /var/tmp/tmc-10.1.4/control.log 2>&1;echo 'PASS Control Center regression'
python3 "$R/identity_qa.py" > /var/tmp/tmc-10.1.4/identity.log 2>&1;echo 'PASS actual ISO identity/payloads'
python3 "$R/bios_installer_qa.py" > /var/tmp/tmc-10.1.4/install.log 2>&1;echo 'PASS BIOS install, HDD reboot, UEFI crossboot'
for fw in uefi bios;do
 export TMC_TEST_FIRMWARE=$fw
 profiles='ps2 ehci companion xhci xhci-hub xhci-hub-kbd xhci-nested-hub'
 if [[ $fw == uefi ]];then profiles+=' uhci ehci-kbd companion-kbd';fi
 for p in $profiles;do
  export TMC_TEST_INPUT=$p TMC_INPUT_RESULT="$R/input-$fw-$p.json"
  python3 tinymacOS/tests/input2_acceptance.py > /var/tmp/tmc-10.1.4/input-$fw-$p.log 2>&1
  echo "PASS live $fw $p"
 done
done
export TMC_INPUT_DISK=$(python3 -c 'import json;print(json.load(open("workspace/release-10.1.4-30B5/bios-installer-result.json"))["external_work"]+"/target/system.raw")')
for fw in uefi bios;do
 export TMC_TEST_FIRMWARE=$fw
 for p in ps2 ehci companion xhci-hub xhci-nested-hub;do
  export TMC_TEST_INPUT=$p TMC_INPUT_RESULT="$R/installed-$fw-$p.json"
  python3 tinymacOS/tests/input2_acceptance.py > /var/tmp/tmc-10.1.4/installed-$fw-$p.log 2>&1
  echo "PASS HDD only $fw $p"
 done
done
unset TMC_INPUT_DISK
python3 "$R/bios_recovery_qa.py" > /var/tmp/tmc-10.1.4/recovery.log 2>&1;echo 'PASS local sealed Recovery restore'
for fw in uefi bios;do
 for p in ps2 xhci-hub-kbd;do
  export TMC_TEST_FIRMWARE=$fw TMC_TEST_INPUT=$p
  python3 "$R/recovery_keyboard_qa.py" > /var/tmp/tmc-10.1.4/recovery-kbd-$fw-$p.log 2>&1
  echo "PASS Italian Recovery $fw $p"
 done
done
