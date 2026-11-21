#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
export TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc10-final TMC_MEM=512 OCR_ROOT=/var/tmp/tmc/tools/root
export TMC_TEST_ISO="$TMC_BUILD/tinymacOS-10.0-OpenCore-Installer-x86_64.iso"
R="$PWD/workspace/release-10.0"
for fw in uefi bios;do
 export TMC_TEST_FIRMWARE=$fw
 profiles='ps2 ehci companion xhci xhci-hub xhci-hub-kbd xhci-nested-hub';if [[ $fw == uefi ]];then profiles+=' uhci ehci-kbd companion-kbd';fi
 for profile in $profiles;do
  export TMC_TEST_INPUT=$profile TMC_INPUT_RESULT="$R/input-$fw-$profile.json"
  python3 tinymacOS/tests/input2_acceptance.py > "/var/tmp/tmc10-final-$fw-$profile.log" 2>&1
  echo "PASS live $fw $profile"
 done
done
python3 "$R/identity_qa.py" > /var/tmp/tmc10-final-identity.log 2>&1;echo 'PASS identity'
python3 "$R/bios_installer_qa.py" > /var/tmp/tmc10-final-installer.log 2>&1;echo 'PASS installer BIOS and installed UEFI crossboot'
export TMC_INPUT_DISK=$(python3 -c 'import json;print(json.load(open("workspace/release-10.0/bios-installer-result.json"))["external_work"]+"/target/system.raw")')
for fw in bios uefi;do
 export TMC_TEST_FIRMWARE=$fw
 for profile in ps2 ehci companion xhci-hub xhci-nested-hub;do
  export TMC_TEST_INPUT=$profile TMC_INPUT_RESULT="$R/installed-$fw-$profile.json"
  python3 tinymacOS/tests/input2_acceptance.py > "/var/tmp/tmc10-final-installed-$fw-$profile.log" 2>&1
  echo "PASS installed no ISO $fw $profile"
 done
done
unset TMC_INPUT_DISK
python3 "$R/bios_recovery_qa.py" > /var/tmp/tmc10-final-recovery.log 2>&1;echo 'PASS local sealed Recovery'
