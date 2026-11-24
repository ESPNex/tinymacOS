#!/bin/bash
# Full original core/HCD/hub enumeration against an EXPLICIT controller MODEL.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/linux-source-contract.py
D=$(mktemp -d /var/tmp/tmc-usbcore-enumeration.XXXXXXXX)
trap 'rm -rf "$D"' EXIT
A=compat/linux/usbcore-include
C=(-std=gnu11 -g -fno-strict-aliasing -fwrapv -ffunction-sections -fdata-sections -fsanitize=address,undefined
 -fno-omit-frame-pointer -fno-pie --param=asan-globals=0 -Werror=implicit-function-declaration
 -I"$A" -Iinclude -include "$A/tmc_kernel.h")
for opt in -O1 -O2;do
 objects=()
 for unit in vendor/linux-v6.12/usbcore-{usb,driver,generic,quirks,endpoint,buffer,port}.c vendor/linux-v6.12/{config,urb,hub,message,hcd}.c compat/linux/usbcore_{services,platform,common}.c tests/linux_usbcore_enumeration.c;do
  name=$(basename "$unit" .c)
  gcc "${C[@]}" "$opt" -MMD -MF "$D/$name.d" -c "$unit" -o "$D/$name.o"
  objects+=("$D/$name.o")
 done
 gcc -g "$opt" -fsanitize=address,undefined -fno-pie -Dtmc_usbcore_os_poll=tmc_usbcore_host_idle -MMD -MF "$D/host.d" -c tests/linux_usbcore_host.c -o "$D/host.o"
 gcc -no-pie -fsanitize=address,undefined -Wl,--gc-sections "${objects[@]}" "$D/host.o" -o "$D/enumeration"
 ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$D/enumeration"
 echo "PASS original core/HCD/hub enumeration MODEL $opt"
done
if [[ -n "${TMC_USBCORE_ENUM_RECEIPT:-}" ]];then
 python3 - "$D" "$TMC_USBCORE_ENUM_RECEIPT" <<'PY'
import hashlib,json,sys
from pathlib import Path
sources={Path('tests/test-linux-usbcore-enumeration.sh'),Path('vendor/linux-v6.12/usbcore-manifest.json')}
for p in Path(sys.argv[1]).glob('*.d'):
    sources.update(Path(n) for n in p.read_text().replace('\\\n',' ').partition(':')[2].split())
result={'passed':True,'scope':'Original complete USB core/HCD/hub runtime on explicit controller MODEL, not QEMU/physical hardware','optimizations':['O1','O2'],'sanitizers':['ASan heap/stack/leaks (globals registration disabled for GC)','UBSan including alignment'],'checks':['root-hub control/status URBs','original hub port debounce/reset and child enumeration','original generic configuration selection','original interface driver binding','3 asynchronous interrupt completions through original HCD BH','hot unplug and full controller teardown with zero allocations remaining'],'physical_transport_tested':False,'usbhid_tested':False,'release_gate':False,'sources':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(sources)}}
Path(sys.argv[2]).write_text(json.dumps(result,indent=2)+'\n')
PY
fi
