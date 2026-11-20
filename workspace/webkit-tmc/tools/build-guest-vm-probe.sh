#!/bin/bash
# Build fresh modified kernel, then a QA-only command volume. NEVER upload it.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OS=$(realpath "$ROOT/../../tinymacOS")
BASE=${TMC_VM_SYSTEM_BUILD:-/var/tmp/tmc-webkit-vm-system}
OUT=${TMC_VM_PROBE_BUILD:-/var/tmp/tmc-webkit-vm-guest}
python3 "$ROOT/tools/sync-system-sources.py" --check
# Persist OS source hash manifest BEFORE compiling the new kernel.
python3 - "$OS" "$ROOT" <<'PY'
import hashlib,json,sys
from pathlib import Path
os,r=map(Path,sys.argv[1:]);names=[]
for p in os.rglob('*'):
 if p.is_file() and p.suffix in ('.c','.h','.S','.sh','.py','.ld') and '__pycache__' not in p.parts:names.append(p)
(r/'test-results/vm-system-source-hashes.json').write_text(json.dumps({str(p.relative_to(os)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(names)},indent=2))
PY
cd "$OS"
TOOLROOT=${TOOLROOT:-/var/tmp/tmc/tools/root} TMC_BUILD="$BASE" bash build.sh
TMC_PROBE_KIND=vm TMC_PROBE_BASE="$BASE" TMC_PROBE_BUILD="$OUT" bash "$ROOT/tools/build-guest-network-probe.sh"
