#!/bin/bash
# Native snapshot implementation tests on disposable RAM, no real disks.
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /var/tmp/tmc-cow-host-XXXXXXXX)
trap 'rm -rf "$out"' EXIT
C=(-O1 -g -ffunction-sections -fdata-sections -fno-builtin -Iinclude -Iarch -Inet/include -Inet/lwip/include)
S=(storage/native_cow.c storage/native_resize.c storage/maintenance.c storage/partition.c kernel/string.c crypto/sha256.c)
for mode in normal sanitized;do
 flags=();if [[ $mode == sanitized ]];then flags=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
 for test in native_cow_host native_cow_fs_host;do
  gcc "${C[@]}" "${flags[@]}" "tests/$test.c" "${S[@]}" -Wl,--gc-sections -o "$out/$test-$mode"
  ASAN_OPTIONS=detect_leaks=1 "$out/$test-$mode"
 done
 gcc "${C[@]}" -O2 "${flags[@]}" -DFV_PAYLOAD=4096 tests/native_cow_vault_host.c "${S[@]}" crypto/vault.c crypto/pbkdf.c crypto/monocypher/monocypher.c -Wl,--gc-sections -o "$out/vault-$mode"
 ASAN_OPTIONS=detect_leaks=1 "$out/vault-$mode"
 echo "PASS native CoW $mode: sharing, historical files, replay, legacy migration, resize, deletion, full pool and real FileVault"
done
