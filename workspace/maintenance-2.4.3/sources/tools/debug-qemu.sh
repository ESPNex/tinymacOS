#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
exec ./tools/run-qemu.sh -serial stdio -monitor none -d guest_errors "$@"
