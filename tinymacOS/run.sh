#!/bin/sh
set -eu
cd "$(dirname "$0")"
exec bash ./tools/run-qemu.sh -serial stdio "$@"
