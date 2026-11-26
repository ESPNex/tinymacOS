#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${TMC_BUILD:?}"
out=$(mktemp -d /var/tmp/tmc-native4k-test-XXXXXXXX);trap 'rm -rf "$out"' EXIT
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -ffunction-sections -fdata-sections -fno-builtin -Iinclude -Iarch -DTMC_FS2_BACKEND -DTMC_MBR_PATH=\""$TMC_BUILD/bios-mbr.bin"\" -DTMC_COEXIST_PATH=\""$TMC_BUILD/bios-coexist.bin"\" "${flags[@]}" tests/native4k_install_host.c fs/tmcbfs2_volume.c storage/fs2_convert.c crypto/vault2.c crypto/pbkdf.c crypto/monocypher/monocypher.c storage/tmcbfs2_seed.c storage/tmcbfs2.c storage/partition.c storage/maintenance.c storage/native_resize.c storage/native_cow.c crypto/sha256.c kernel/string.c arch/biosdisk.S -Wl,--gc-sections -o "$out/$mode"
 UBSAN_OPTIONS=halt_on_error=1 "$out/$mode"
 echo "PASS native 4Kn install and conversion adapter $mode"
done
