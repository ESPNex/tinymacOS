#!/usr/bin/env python3
"""Create an entirely blank raw installation target. Never touch an existing file."""
import argparse
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('path',type=Path);p.add_argument('--mib',type=int,default=96);a=p.parse_args()
if not 64<=a.mib<=131072:p.error('Allowed size: 64 to 131072 MiB')
a.path.parent.mkdir(exist_ok=True,parents=True)
with a.path.open('xb') as f:f.truncate(a.mib<<20)
print('Blank SATA target:',a.path,a.mib,'MiB')
