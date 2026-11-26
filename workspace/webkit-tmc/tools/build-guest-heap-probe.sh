#!/bin/bash
# Build current production sources first; no historical kernel overlay applied.
set -euo pipefail
export TOOLROOT=${TOOLROOT:-/var/tmp/tmc-tools}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OS=$(realpath "$ROOT/../../tinymacOS")
BASE=${TMC_VM_SYSTEM_BUILD:-/var/tmp/tmc-webkit-heap-system}
OUT=${TMC_PROBE_BUILD:-/var/tmp/tmc-webkit-heap-guest}
python3 "$ROOT/tools/sync-system-sources.py" --check
python3 - "$OS" "$ROOT" <<'PY'
import hashlib,json,sys,os
from pathlib import Path
system,root=map(Path,sys.argv[1:]);files=sorted(p for p in system.rglob('*') if p.is_file() and p.suffix in ('.c','.h','.S','.sh','.py','.ld') and '__pycache__' not in p.parts)
results=Path(os.environ.get('TMC_QA_RESULTS',str(root/'test-results')));results.mkdir(parents=True,exist_ok=True)
(results/'vm-system-source-hashes.json').write_text(json.dumps({str(p.relative_to(system)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},indent=2))
PY
cd "$OS"
TOOLROOT=${TOOLROOT:-/var/tmp/tmc-tools} TMC_BUILD="$BASE" bash build.sh
TMC_PROBE_KIND=heap TMC_PROBE_BASE="$BASE" TMC_PRODUCTION_BUILD="$BASE" TMC_PROBE_BUILD="$OUT" bash "$ROOT/tools/build-guest-network-probe.sh"
