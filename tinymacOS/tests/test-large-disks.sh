#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${TMC_BUILD:?External build directory required}"
out=$(mktemp -d /var/tmp/tmc-large-disks-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
python3 tools/bios-image.py "$TMC_BUILD"
C=(-O1 -g -ffunction-sections -fdata-sections -fno-builtin -Iinclude -Iarch -DTMC_MBR_PATH=\""$TMC_BUILD/bios-mbr.bin"\" -DTMC_COEXIST_PATH=\""$TMC_BUILD/bios-coexist.bin"\")
S=(tests/large_disk_host.c fs/tmcbfs2_volume.c storage/fs2_convert.c crypto/vault2.c crypto/pbkdf.c crypto/monocypher/monocypher.c storage/tmcbfs2_seed.c storage/tmcbfs2.c storage/partition.c storage/maintenance.c crypto/sha256.c kernel/string.c arch/biosdisk.S)
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc "${C[@]}" "${flags[@]}" "${S[@]}" -Wl,--gc-sections -o "$out/$mode"
 ASAN_OPTIONS=detect_leaks=1 "$out/$mode"
 echo "PASS 1TB/16TiB actual installer $mode"
done
