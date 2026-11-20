#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
export TOOLROOT=/var/tmp/tmc/tools/root OCR_ROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc245-input4-final TMC_MEM=512
export TMC_TEST_ISO="$TMC_BUILD/tinymacOS-2.4.5-OpenCore-Installer-x86_64.iso"
for fw in uefi bios;do
 for p in ps2 xhci-hub-kbd;do
  export TMC_TEST_FIRMWARE=$fw TMC_TEST_INPUT=$p
  python3 workspace/release-2.4.5-input4/italian_apps_qa.py > /var/tmp/tmc-input4-italian-$fw-$p.log 2>&1
  echo "PASS Italian apps $fw $p"
  python3 workspace/release-2.4.5-input4/recovery_keyboard_qa.py > /var/tmp/tmc-input4-rec-$fw-$p.log 2>&1
  echo "PASS Italian Recovery $fw $p"
 done
done
export TMC_UI_RESULT="$PWD/workspace/release-2.4.5-input4/chrome-result.json"
python3 tinymacOS/tests/mission_chrome.py > /var/tmp/tmc-input4-chrome.log 2>&1
echo 'PASS Mission Control and menubar regression'
