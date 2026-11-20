#!/usr/bin/env python3
"""Run the 2.4.3 retail acceptance suites with fresh, dedicated external storage."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--work', type=Path, help='New external directory for QA disks and evidence (default: a new /var/tmp directory).')
args = parser.parse_args()
if args.work:
    work = args.work.resolve()
    if any(work == x or x in work.parents for x in (ROOT, Path('/home/user').resolve())):
        parser.error('The QA work directory must be outside the workspace; use /var/tmp.')
    work.mkdir(parents=True, exist_ok=False)
else:
    work = Path(tempfile.mkdtemp(prefix='qa-riviera-', dir='/var/tmp'))
env = os.environ.copy()
env.setdefault('TMC_BUILD', '/var/tmp/tinymacos-%d/2.4.3' % os.getuid())
iso = Path(env['TMC_BUILD'])/'tinymacOS-2.4.3-x86_64.iso'
if not iso.is_file():
    parser.error('Build the ISO first: ./build.sh (same TMC_BUILD).')
for key in ('ISO', 'DISK', 'LEGACY_DATA', 'RECOVERY_IMG', 'STATE', 'BOOT_DISK', 'LIVE'):
    env.pop(key, None)
print('External QA directory:', work, flush=True)
disk = work/'live/sata/system.raw'
for suite, argv in [
    ('riviera_live', [work/'live']), ('riviera_guard', [work/'guard']),
    ('riviera_contained',[iso,disk,work/'contained']),
    ('riviera_pointer', [work/'pointer','--disk',disk]),
    ('riviera_disk', [disk, work/'disk']), ('riviera_apps', [disk, work/'apps']),
    ('riviera_integrity', [work/'disk/installed.raw', work/'integrity']),
    ('riviera_runtime', [disk, work/'runtime']), ('crypto', []),
]:
    subprocess.run([sys.executable, '-u', ROOT/'tests'/(suite+'.py'), *argv], cwd=ROOT, env=env, check=True)
subprocess.run(['cc','-O2','-Iinclude','tests/tablet.c','drivers/hidtablet.c','-o',work/'hid-test'],cwd=ROOT,check=True)
subprocess.run([work/'hid-test'],check=True)
print('Riviera: all selected suites completed. Local QA disks are retained in', work)
