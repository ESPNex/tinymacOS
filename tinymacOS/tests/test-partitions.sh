#!/bin/bash
# No physical block device or user-owned disk is opened by this test.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${TMC_BUILD:?External build directory required}"
out=$(mktemp -d /var/tmp/tmc-partition-test-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
python3 tools/bios-image.py "$TMC_BUILD"
C=(-O1 -g -ffunction-sections -fdata-sections -fno-builtin -Iinclude -Iarch
 -DTMC_MBR_PATH=\""$TMC_BUILD/bios-mbr.bin"\" -DTMC_COEXIST_PATH=\""$TMC_BUILD/bios-coexist.bin"\")
S=(tests/partition_install_host.c storage/partition.c storage/maintenance.c storage/native_resize.c storage/native_cow.c crypto/sha256.c kernel/string.c arch/biosdisk.S)
gcc "${C[@]}" "${S[@]}" -Wl,--gc-sections -o "$out/test"
"$out/test"
gcc "${C[@]}" -fsanitize=address,undefined -fno-omit-frame-pointer "${S[@]}" -Wl,--gc-sections -o "$out/sanitized"
ASAN_OPTIONS=detect_leaks=1 "$out/sanitized"
