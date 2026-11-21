#!/bin/bash
# Disposable QA image, NOT a production release: all command aliases execute
# the network probe. Never upload this image as a working browser/system.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OS=$(realpath "$ROOT/../../tinymacOS")
KIND=${TMC_PROBE_KIND:-network}
case "$KIND" in network|vm|smp|tls|sync|threads) ;; *) echo 'Unknown probe kind' >&2;exit 2;; esac
BASE=${TMC_PROBE_BASE:-/var/tmp/tmc-maintenance-241}
OUT=${TMC_PROBE_BUILD:-/var/tmp/tmc-webkit-network-guest}
python3 "$OS/tools/media_path.py" "$OUT" "$OUT/tinymacOS-qa-probe-x86_64.iso"
mkdir -p "$OUT"
# Preserve exactly the source hashes used BEFORE any target compilation.
python3 - "$ROOT" "$OS" "$OUT" "$KIND" <<'PY'
from pathlib import Path
import sys,hashlib,json
r,o,d=map(Path,sys.argv[1:4]);kind=sys.argv[4];files=[r/('tests/guest_'+kind+'_probe.c'),*sorted((r/'posix/src').glob('*.c')),*sorted((r/'posix/include').glob('*.h')),*sorted((r/'posix/libc-headers').rglob('*.h')),o/'userspace/lib.h',o/'userspace/start.S',o/'userspace/layout.ld',o/'kernel/string.c',o/'crypto/sha256.c',o/'kernel/net.c',o/'kernel/syscall.c',*sorted((o/'include').glob('*.h'))]
if kind=='threads':files += [r/'tests/thread_cases.h',r/'tests/threads_cpp_smoke.cpp']
manifest={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
(r/('test-results/guest-'+kind+'-source-hashes.json')).write_text(json.dumps(manifest,indent=2))
PY
cd "$OS"
source tools/env.sh
if [ ! -f "$BASE/kernel.elf" ];then
 TOOLROOT=${TOOLROOT:-/var/tmp/tmc/tools/root} TMC_BUILD="$BASE" bash build.sh
fi
for f in "$BASE/"*.elf "$BASE/"*.EFI "$BASE/"kext-*.o "$BASE/KernelCache"; do cp "$f" "$OUT/"; done
C=(-std=gnu11 -O2 -ffreestanding -fno-stack-protector -fno-builtin -fno-asynchronous-unwind-tables -fno-unwind-tables -mno-red-zone -mgeneral-regs-only -fpie -fno-plt -nostdinc -isystem "$(gcc -print-file-name=include)" -I"$ROOT/posix/libc-headers" -I"$ROOT/posix/include" -Iinclude -Iuserspace -Wall -Wextra -Werror)
OBJS=()
for f in native_syscall network clock_entropy poll descriptors vm tls errno sync threads; do
 gcc "${C[@]}" -c "$ROOT/posix/src/$f.c" -o "$OUT/probe-$f.o"
 OBJS+=("$OUT/probe-$f.o")
done
gcc "${C[@]}" -c "$ROOT/tests/guest_${KIND}_probe.c" -o "$OUT/probe-main.o"
OBJS+=("$OUT/probe-main.o")
if [[ "$KIND" == threads ]];then
 g++ "${C[@]/-std=gnu11/-std=c++20}" -nostdinc++ -fno-exceptions -fno-rtti -c "$ROOT/tests/threads_cpp_smoke.cpp" -o "$OUT/probe-cpp.o"
 OBJS+=("$OUT/probe-cpp.o")
fi
for f in userspace/start.S kernel/string.c crypto/sha256.c;do
 gcc "${C[@]}" -c "$f" -o "$OUT/probe-${f##*/}.o"
 OBJS+=("$OUT/probe-${f##*/}.o")
done
ld -nostdlib --build-id=none -z noexecstack -z max-page-size=0x1000 -T userspace/layout.ld "${OBJS[@]}" -o "$OUT/command.debug"
objcopy --strip-all "$OUT/command.debug" "$OUT/command.elf"
python3 tools/tramontana-assets.py "$OUT"
python3 tools/control-svg.py "$OUT"
python3 tools/system_volume.py "$OUT" disk
python3 tools/system_volume.py "$OUT" live
TOOLROOT=${TOOLROOT:-/var/tmp/tmc/tools/root} TMC_BUILD="$OUT" bash tools/iso.sh
# Stable QA name is created ONLY from the ISO emitted by this build.
mv -f "$OUT/$TMC_ISO" "$OUT/tinymacOS-qa-probe-x86_64.iso"
python3 - "$ROOT" "$OUT" "$KIND" <<'PY'
from pathlib import Path
import hashlib,json,sys,subprocess,os
r,d=map(Path,sys.argv[1:3]);kind=sys.argv[3];manifest={'qa_only':True,'gcc':subprocess.check_output(['gcc','--version'],text=True).splitlines()[0],'sha256':{name:hashlib.sha256((d/name).read_bytes()).hexdigest() for name in ('command.elf','kernel.elf','KernelCache','tinymacOS-qa-probe-x86_64.iso')}}
production=Path(os.environ.get('TMC_PRODUCTION_BUILD','/var/tmp/tmc-webkit-vm-system'))
for name in ('kernel.elf','KernelCache'):assert (d/name).read_bytes()==(production/name).read_bytes(), 'QA kernel differs from current production build'
manifest['iso_filename']='tinymacOS-qa-probe-x86_64.iso';manifest['production_build']=str(production);manifest['kernel_equals_current_production']=True
if kind=='threads':manifest['g++']=subprocess.check_output(['g++','--version'],text=True).splitlines()[0]
(r/('test-results/guest-'+kind+'-build.json')).write_text(json.dumps(manifest,indent=2))
PY
printf '\nQA ONLY - DO NOT DISTRIBUTE: %s\n' "$OUT/tinymacOS-qa-probe-x86_64.iso"
