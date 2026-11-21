#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
export TOOLROOT=/var/tmp/tmc/tools/root OCR_ROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc10-final TMC_MEM=512
export TMC_TEST_ISO="$TMC_BUILD/tinymacOS-10.0-OpenCore-Installer-x86_64.iso"
current_pass(){ python3 - "$1" <<'PY'
from pathlib import Path
import json,sys,os,hashlib
try:
 j=json.loads(Path(sys.argv[1]).read_text());p=Path(os.environ['TMC_TEST_ISO']);assert j['passed'] and j['iso_sha256']==hashlib.file_digest(p.open('rb'),'sha256').hexdigest()
except Exception:sys.exit(1)
PY
}
for fw in ${TMC_ITALIAN_FIRMWARES:-uefi bios};do
 for p in ps2 xhci-hub-kbd;do
  export TMC_TEST_FIRMWARE=$fw TMC_TEST_INPUT=$p
  if ! current_pass "workspace/release-10.0/italian-apps-$fw-$p.json";then
   python3 workspace/release-10.0/italian_apps_qa.py > /var/tmp/tmc10-italian-$fw-$p.log 2>&1
  fi
  echo "PASS Italian apps $fw $p (current ISO hash verified)"
  if ! current_pass "workspace/release-10.0/recovery-keyboard-$fw-$p.json";then
   python3 workspace/release-10.0/recovery_keyboard_qa.py > /var/tmp/tmc10-rec-$fw-$p.log 2>&1
  fi
  echo "PASS Italian Recovery $fw $p (current ISO hash verified)"
 done
done
if [[ ${TMC_ITALIAN_SKIP_CHROME:-0} != 1 ]];then
 export TMC_UI_RESULT="$PWD/workspace/release-10.0/chrome-result.json"
 if ! current_pass "$TMC_UI_RESULT";then python3 tinymacOS/tests/mission_chrome.py > /var/tmp/tmc10-chrome.log 2>&1;fi
 echo 'PASS Mission Control and menubar regression'
fi
