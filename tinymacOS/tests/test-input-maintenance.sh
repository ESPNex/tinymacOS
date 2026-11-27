#!/bin/sh
# Current queue/routing contracts. Retired native USB class fixtures live in Git history.
set -eu
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-input-host.XXXXXXXX)
trap 'rm -rf "$out"' EXIT
C='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-sign-compare -Iinclude'
for t in click_pair input_maintenance keyboard_it usbroute prefs_plist;do
 gcc $C tests/$t.c -o "$out/$t"
 "$out/$t"
done
python3 tools/linux-source-contract.py
