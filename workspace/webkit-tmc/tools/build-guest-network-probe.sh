#!/bin/bash
# Disposable QA image, NOT a production release: all command aliases execute
# the network probe. Never upload this image as a working browser/system.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OS=$(realpath "$ROOT/../../tinymacOS")
KIND=${TMC_PROBE_KIND:-network}
case "$KIND" in network|vm|smp|tls|sync|threads|heap|cpp|lifecycle) ;; *) echo 'Unknown probe kind' >&2;exit 2;; esac
BASE=${TMC_PROBE_BASE:-/var/tmp/tmc-maintenance-241}
OUT=${TMC_PROBE_BUILD:-/var/tmp/tmc-webkit-network-guest}
python3 "$OS/tools/media_path.py" "$OUT" "$OUT/tinymacOS-qa-probe-x86_64.iso"
mkdir -p "$OUT"
# Preserve exactly the source hashes used BEFORE any target compilation.
python3 - "$ROOT" "$OS" "$OUT" "$KIND" <<'PY'
from pathlib import Path
import sys,hashlib,json,os
r,o,d=map(Path,sys.argv[1:4]);kind=sys.argv[4];files=[r/('tests/guest_'+kind+'_probe.c'),*sorted((r/'posix/src').glob('*.c')),*sorted((r/'posix/include').glob('*.h')),*sorted((r/'posix/libc-headers').rglob('*.h')),o/'userspace/lib.h',o/'userspace/start.S',o/'userspace/layout.ld',o/'kernel/string.c',o/'crypto/sha256.c',o/'kernel/net.c',o/'kernel/syscall.c',*sorted((o/'include').glob('*.h'))]
if kind=='threads':files += [r/'tests/thread_cases.h',r/'tests/threads_cpp_smoke.cpp']
if kind=='heap':files += [r/'tests/heap_cases.h',r/'tests/heap_cpp_smoke.cpp']
if kind=='lifecycle':files += [r/'tests/lifecycle_cases.h',r/'tests/lifecycle_cases.cpp',r/'cpp/lifecycle.ld',*sorted((r/'cpp/src').glob('*.cpp')),*sorted((r/'cpp/include').glob('*'))]
if kind=='cpp':files += [r/'tests/cpp_cases.h',r/'tests/cpp_cases.cpp',*sorted((r/'cpp/src').glob('*.cpp')),*sorted((r/'cpp/include').glob('*'))]
files += [r/'tools/build-guest-network-probe.sh']
manifest={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
(Path(os.environ.get('TMC_QA_RESULTS',str(r/'test-results')))/('guest-'+kind+'-source-hashes.json')).write_text(json.dumps(manifest,indent=2))
PY
cd "$OS"
source tools/env.sh
if [ ! -f "$BASE/kernel.elf" ];then
 TOOLROOT=${TOOLROOT:-/var/tmp/tmc/tools/root} TMC_BUILD="$BASE" bash build.sh
fi
for f in "$BASE/"*.elf "$BASE/"*.EFI "$BASE/"kext-*.o "$BASE/KernelCache"; do cp "$f" "$OUT/"; done
C=(-std=gnu11 -O2 -ffreestanding -fno-stack-protector -fno-builtin -fno-asynchronous-unwind-tables -fno-unwind-tables -mno-red-zone -mgeneral-regs-only -fpie -fno-plt -nostdinc -isystem "$(gcc -print-file-name=include)" -I"$ROOT/posix/libc-headers" -I"$ROOT/posix/include" -I"$ROOT/tests" -Iinclude -Iuserspace -Wall -Wextra -Werror)
if [[ "$KIND" == lifecycle ]];then C+=(-I"$ROOT/cpp/include");fi
OBJS=()
SOURCES=(native_syscall network clock_entropy poll descriptors vm tls errno sync threads)
if [[ "$KIND" == heap || "$KIND" == cpp || "$KIND" == lifecycle ]];then SOURCES+=(heap libc_alloc);fi
for f in "${SOURCES[@]}"; do
 gcc "${C[@]}" -c "$ROOT/posix/src/$f.c" -o "$OUT/probe-$f.o"
 OBJS+=("$OUT/probe-$f.o")
done
gcc "${C[@]}" -c "$ROOT/tests/guest_${KIND}_probe.c" -o "$OUT/probe-main.o"
OBJS+=("$OUT/probe-main.o")
if [[ "$KIND" == threads || "$KIND" == heap ]];then
 cpp="$ROOT/tests/threads_cpp_smoke.cpp";[[ "$KIND" != heap ]] || cpp="$ROOT/tests/heap_cpp_smoke.cpp"
 g++ "${C[@]/-std=gnu11/-std=c++20}" -nostdinc++ -fno-exceptions -fno-rtti -c "$cpp" -o "$OUT/probe-cpp.o"
 OBJS+=("$OUT/probe-cpp.o")
fi
if [[ "$KIND" == cpp || "$KIND" == lifecycle ]];then
 CPP_SOURCES=(cpp/src/new.cpp cpp/src/guard.cpp tests/cpp_cases.cpp)
 if [[ "$KIND" == lifecycle ]];then CPP_SOURCES=(cpp/src/new.cpp cpp/src/guard.cpp cpp/src/lifecycle.cpp tests/lifecycle_cases.cpp);fi
 for source in "${CPP_SOURCES[@]}";do
  name=$(basename "$source")
  g++ "${C[@]/-std=gnu11/-std=c++20}" -I"$ROOT/cpp/include" -I"$ROOT/tests" -nostdinc++ -fno-exceptions -fno-rtti -c "$ROOT/$source" -o "$OUT/probe-$name.o"
  OBJS+=("$OUT/probe-$name.o")
 done
fi
for f in userspace/start.S kernel/string.c crypto/sha256.c;do
 gcc "${C[@]}" -c "$f" -o "$OUT/probe-${f##*/}.o"
 OBJS+=("$OUT/probe-${f##*/}.o")
done
# Cross-language callback pointers live above 32-bit absolute address range.
# Keep their valid GOT loads instead of ld relaxation to a sign-extended imm32.
RELAX=();LAYOUT=userspace/layout.ld
if [[ "$KIND" == cpp || "$KIND" == lifecycle ]];then RELAX=(--no-relax);fi
if [[ "$KIND" == lifecycle ]];then LAYOUT="$ROOT/cpp/lifecycle.ld";fi
ld "${RELAX[@]}" -nostdlib --build-id=none -z noexecstack -z max-page-size=0x1000 -T "$LAYOUT" "${OBJS[@]}" -o "$OUT/command.debug"
objcopy --strip-all "$OUT/command.debug" "$OUT/command.elf"
python3 tools/tramontana-assets.py "$OUT"
python3 tools/control-svg.py "$OUT"
python3 tools/system_volume.py "$OUT" disk
python3 tools/system_volume.py "$OUT" live
# Recovery is sealed into the copied EFI loaders; preserve its original bytes.
cp "$BASE/System-recovery.vol" "$OUT/System-recovery.vol"
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
if kind in ('threads','heap','cpp','lifecycle'):manifest['g++']=subprocess.check_output(['g++','--version'],text=True).splitlines()[0]
(Path(os.environ.get('TMC_QA_RESULTS',str(r/'test-results')))/('guest-'+kind+'-build.json')).write_text(json.dumps(manifest,indent=2))
PY
printf '\nQA ONLY - DO NOT DISTRIBUTE: %s\n' "$OUT/tinymacOS-qa-probe-x86_64.iso"
