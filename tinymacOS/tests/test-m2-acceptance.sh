#!/bin/bash
# Host gates only. Native guest/cold-boot tests are m2_capacity_guest.py.
# Every target is a disposable fixture; never pass a physical/user disk.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${TMC_BUILD:?External build containing the BIOS images is required}"
export PATH=/usr/sbin:$PATH
# The retired generation-1 native resize test no longer exists. Never restore
# its backend; TMCFS2 install/runtime/import/encryption and refusal gates follow.
for suite in display-size m2-geometry windows-gaps partitions large-disks native4k-install diskedit fs2-install fs2-runtime fs2-import-adapter fs2-convert nvme; do
    bash "tests/test-$suite.sh"
done
printf '%s\n' 'PASS M2 host gates; real guest >2TB/4Kn/three-view cold-boot acceptance remains a separate required gate'
