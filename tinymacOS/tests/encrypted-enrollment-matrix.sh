#!/bin/bash
# Development macroblock only: disposable images, no release/push/deployment.
# Requires the regular build tools, QEMU/OVMF, tesseract+English data, and
# Python PIL/numpy/opencv/fonttools/PyNaCl in the selected environment.
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/env.sh
export TMC_FS2_BACKEND=1 PYTHONDONTWRITEBYTECODE=1
export TMC_FONT="${TMC_FONT:-$PWD/assets/type.tfn}"
export TMC_QA_DIR="${TMC_QA_DIR:-$(mktemp -d /var/tmp/tmc-enrollment-matrix-XXXXXXXX)}"
export TMC_STAGE_ISO="$TMC_BUILD/$TMC_ISO"
mkdir -p "$TMC_QA_DIR"
python3 tools/bios-image.py "$TMC_BUILD" > "$TMC_QA_DIR/bios-image.log" 2>&1
for group in fs2-install installer-flow vault2 vault fs2-runtime partitions native-resize large-disks; do
    echo "RUN host $group (normal + sanitizers)"
    bash "tests/test-$group.sh" > "$TMC_QA_DIR/host-$group.log" 2>&1
    echo "PASS host $group"
done
echo 'RUN complete native kernel + Recovery build'
bash build.sh > "$TMC_QA_DIR/build.log" 2>&1
for gate in encrypted_install_reset fs2_install fs2_backup_ui fs2_reset; do
    echo "RUN real GUI $gate"
    python3 "tests/${gate}_guest.py" > "$TMC_QA_DIR/guest-$gate.log" 2>&1
    echo "PASS real GUI $gate"
done
sha256sum "$TMC_STAGE_ISO" "$TMC_BUILD/KernelCache" > "$TMC_QA_DIR/artifacts.sha256"
echo "PASS encrypted enrollment macroblock; exact receipts in $TMC_QA_DIR"
echo 'Not full release acceptance: conversion, external backup import, native 4Kn and final release matrix remain separate.'
