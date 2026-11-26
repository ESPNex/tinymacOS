#!/bin/bash
# Fresh native FS2 base plus opt-in C++ lifecycle diagnostic; NEVER distribute.
set -euo pipefail
export TOOLROOT=${TOOLROOT:-/var/tmp/tmc-tools}
export TMC_FS2_BACKEND=1 TMC_USB_CANONICAL=0
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OS=$(realpath "$ROOT/../../tinymacOS")
BASE=${TMC_LIFECYCLE_SYSTEM_BUILD:-/var/tmp/tmc-webkit-lifecycle-system}
OUT=${TMC_PROBE_BUILD:-/var/tmp/tmc-webkit-lifecycle-guest}
: "${TMC_QA_RESULTS:?Use an external results directory}"
mkdir -p "$TMC_QA_RESULTS"
python3 "$ROOT/tools/sync-system-sources.py" --check
python3 - "$OS" "$TMC_QA_RESULTS" <<'PY'
import hashlib,json,sys
from pathlib import Path
system,results=map(Path,sys.argv[1:]);files=sorted(p for p in system.rglob('*') if p.is_file() and p.suffix in ('.c','.h','.S','.sh','.py','.ld') and '__pycache__' not in p.parts)
(results/'vm-system-source-hashes.json').write_text(json.dumps({str(p.relative_to(system)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},indent=2))
PY
cd "$OS"
TMC_BUILD="$BASE" bash build.sh
TMC_PROBE_KIND=lifecycle TMC_PROBE_BASE="$BASE" TMC_PRODUCTION_BUILD="$BASE" TMC_PROBE_BUILD="$OUT" bash "$ROOT/tools/build-guest-network-probe.sh"
