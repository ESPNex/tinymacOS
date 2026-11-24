#!/bin/bash
# Canonical-ABI development contract, NOT a kernel/USB enumeration gate.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/linux-source-contract.py
python3 - <<'PY'
from pathlib import Path
import hashlib,json
manifest=json.loads(Path('vendor/linux-v6.12/usbcore-manifest.json').read_text())
for row in manifest['files']:
    p=Path('..')/row['path']
    assert hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256'],p
print('PASS exact upstream canonical ABI sources:',len(manifest['files']))
PY
out=$(mktemp -d /var/tmp/tmc-usbcore-host.XXXXXXXX)
trap 'rm -rf "$out"' EXIT
abi=compat/linux/usbcore-include
common=(-std=gnu11 -g -fwrapv -ffunction-sections -fdata-sections
 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie
 --param=asan-globals=0 -Werror=implicit-function-declaration
 -Wno-address-of-packed-member -I"$abi" -Iinclude -include "$abi/tmc_kernel.h")
# ASan global registration is disabled ONLY to allow gc-sections to discard
# uncalled upstream registration tables. Heap/stack ASan and all UBSan checks
# remain enabled. The linked/tested subset is explicitly recorded below.
for opt in -O1 -O2; do
 gcc "${common[@]}" "$opt" -Wall -Wextra -Werror -MMD -MF "$out/services.d" -c compat/linux/usbcore_services.c -o "$out/services.o"
 gcc "${common[@]}" "$opt" -MMD -MF "$out/test.d" -c tests/linux_usbcore.c -o "$out/test.o"
 gcc -std=gnu11 -g "$opt" -fsanitize=address,undefined -fno-pie -MMD -MF "$out/host.d" -c tests/linux_usbcore_host.c -o "$out/host.o"
 objects=("$out/services.o" "$out/test.o" "$out/host.o")
 for unit in usb driver quirks; do
  gcc "${common[@]}" "$opt" -MMD -MF "$out/$unit.d" -c "vendor/linux-v6.12/usbcore-$unit.c" -o "$out/$unit.o"
  objects+=("$out/$unit.o")
 done
 for unit in config urb hub; do
  gcc "${common[@]}" "$opt" -MMD -MF "$out/$unit.d" -c "vendor/linux-v6.12/$unit.c" -o "$out/$unit.o"
  objects+=("$out/$unit.o")
 done
 # The second executable exercises ORIGINAL USB bus probe/unbind/dynamic-ID
 # lifecycle. Its NO-I/O tripwires abort on every hardware call, not success.
 binding_objects=("$out/services.o" "$out/host.o" "$out/usb.o" "$out/driver.o" "$out/quirks.o" "$out/config.o" "$out/urb.o" "$out/hub.o")
 for unit in vendor/linux-v6.12/message.c vendor/linux-v6.12/usbcore-generic.c vendor/linux-v6.12/usbcore-endpoint.c tests/linux_usbcore_binding.c tests/linux_usbcore_noio.c; do
  name=$(basename "$unit" .c)
  gcc "${common[@]}" "$opt" -MMD -MF "$out/$name.d" -c "$unit" -o "$out/$name.o"
  binding_objects+=("$out/$name.o")
 done
 gcc -no-pie -fsanitize=address,undefined -Wl,--gc-sections "${binding_objects[@]}" -o "$out/binding-test"
 ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$out/binding-test"
 gcc -no-pie -fsanitize=address,undefined -Wl,--gc-sections "${objects[@]}" -o "$out/canonical-test"
 for symbol in usb_match_id usb_get_configuration usb_ifnum_to_if usb_hub_to_struct_hub usb_device_is_owned usb_alloc_urb; do
  nm "$out/canonical-test" | grep -E " T ${symbol}$" >/dev/null
 done
 ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$out/canonical-test"
 echo "PASS canonical ABI $opt ASan/UBSan; full core/hub/usbhid runtime NOT certified"
done
if [[ -n "${TMC_USBCORE_RECEIPT:-}" ]]; then
 python3 - "$out" "$TMC_USBCORE_RECEIPT" <<'PY'
from pathlib import Path
import hashlib,json,sys
root=Path.cwd();scratch=Path(sys.argv[1]);sources={Path('tests/test-linux-usbcore.sh'),Path('tools/linux-source-contract.py'),Path('vendor/linux-v6.12/manifest.json'),Path('vendor/linux-v6.12/usbcore-manifest.json')}
for dep in scratch.glob('*.d'):
    text=dep.read_text().replace('\\\n',' ')
    sources.update(Path(p) for p in text.partition(':')[2].split())
hashes={str(p):hashlib.sha256((root/p).read_bytes()).hexdigest() for p in sorted(sources)}
result={'passed':True,'scope':'canonical USB ABI development host contracts only','optimizations':['O1','O2'],
 'sanitizers':['ASan heap/stack/leaks; global registration disabled for section GC','UBSan (including alignment)'],
 'whole_original_units_compiled':['usb.c','driver.c','quirks.c','config.c','urb.c','hub.c','message.c','generic.c','endpoint.c'],
 'original_code_exercised':['device/interface/version ID matching','configuration descriptor parsing and allocation-failure unwind','alternate/interface lookup','hub ownership queries','URB allocation/reference/release','original USB bus probe/unbind and authorization','original dynamic-ID add/show/remove','original driver registration allocation-failure unwind'],
 'native_services_exercised':['device/bus/driver registration and bind/unbind','parent/reference ownership','deletion-safe device iteration','allocation-failure rollback','managed resource release','attribute registry and metadata groups','independent BH versus sleeping workers','work/timer self-rearm and callback-owned frees','delayed cancellation','completion deadlines','UTF16 decoding'],
 'descriptor_transport':'explicit host fixture; not an actual USB device','binding_transport':'NO-I/O; any hardware call aborts the test',
 'original_binding_host_O1_O2':True,
 'kernel_linked':False,'full_core_runtime_tested':False,'hub_enumeration_tested':False,'usbhid_tested':False,'release_gate':False,'published':False,'source_files':hashes}
path=Path(sys.argv[2]);path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(result,indent=2)+'\n')
print('Source-bound development receipt:',path)
PY
fi
