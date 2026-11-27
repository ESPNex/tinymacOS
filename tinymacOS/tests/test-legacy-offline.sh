#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${TMC_BUILD:?External build directory required}"
out=$(mktemp -d /var/tmp/tmc-legacy-offline-XXXXXX);trap 'rm -rf "$out"' EXIT
S=(tests/legacy_offline_host.c fs/tmcbfs2_volume.c storage/fs2_convert.c crypto/vault2.c crypto/pbkdf.c crypto/monocypher/monocypher.c storage/tmcbfs2_seed.c storage/tmcbfs2.c storage/partition.c storage/maintenance.c crypto/sha256.c kernel/string.c arch/biosdisk.S)
for mode in normal sanitized;do
 flags=();[[ $mode != sanitized ]] || flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
 gcc -O1 -g -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iarch -DTMC_MBR_PATH=\""$TMC_BUILD/bios-mbr.bin"\" -DTMC_COEXIST_PATH=\""$TMC_BUILD/bios-coexist.bin"\" "${flags[@]}" "${S[@]}" -Wl,--gc-sections -o "$out/$mode"
 UBSAN_OPTIONS=halt_on_error=1 "$out/$mode"
done
