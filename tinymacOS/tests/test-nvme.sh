#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-nvme-host-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fno-builtin -Iinclude "${flags[@]}" tests/nvme_host.c storage/nvme_native.c kernel/string.c -o "$out/$mode"
 "$out/$mode"
 echo "PASS NVMe transport $mode"
done
