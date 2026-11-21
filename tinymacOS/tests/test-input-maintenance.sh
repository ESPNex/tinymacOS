#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-input-host.XXXXXXXX)
trap 'rm -rf "$out"' EXIT
C='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Iinclude'
gcc $C tests/input_maintenance.c drivers/hidconfig.c drivers/hidtablet.c -o "$out/queue"
"$out/queue"
# Substitute only the MMIO notification in the host transport. No OS source edit.
python3 - "$out/xhci-test.c" <<'PY'
from pathlib import Path
import sys
p=Path('drivers/xhci.c').resolve();s=p.read_text()
assert s.count('db[s]=ep;')==1
s=s.replace('db[s]=ep;','db[s]=ep;test_doorbell(s,ep);')
s=s.replace('"../tests/input_test_support.h"','"'+str(p.parents[1]/'tests/input_test_support.h')+'"')
s=s.replace('"../kernel/k.h"','"'+str(p.parents[1]/'kernel/k.h')+'"')
Path(sys.argv[1]).write_text(s)
PY
gcc $C -DTMC_XHCI_TEST_SOURCE=\""$out/xhci-test.c"\" tests/input_enumeration.c drivers/hidconfig.c drivers/hidtablet.c -o "$out/enumeration"
"$out/enumeration"
for t in hidconfig relative_pointer; do gcc $C "tests/$t.c" drivers/hidconfig.c drivers/hidtablet.c -o "$out/$t";"$out/$t";done

gcc $C -DTMC_XHCI_TEST_SOURCE=\""$out/xhci-test.c"\" tests/xhci_hubs.c drivers/hidconfig.c drivers/hidtablet.c -o "$out/hubs"
"$out/hubs"
gcc $C tests/usbroute.c -o "$out/route"
"$out/route"

gcc $C tests/keyboard_it.c -o "$out/keyboard-it";"$out/keyboard-it"

gcc $C tests/recovery_usb.c drivers/hidtablet.c -o "$out/recovery-usb"
"$out/recovery-usb"
