#!/usr/bin/env python3
"""Conservative preflight: count ALL files, even caches excluded by snapshots.

A passing result checks local size/count only; it cannot certify that the
platform has completed a snapshot. Keep margin below 128 MiB / 10,000 files.
"""
from pathlib import Path
import json
import sys
root = Path('/home/user')
files = [p for p in root.rglob('*') if p.is_file()]
size = sum(p.stat().st_size for p in files)
result = {'files': len(files), 'bytes': size, 'MB': round(size / 1000000, 2),
          'MiB': round(size / 1048576, 2),
          'user_limit_bytes': 128000000, 'bytes_below_user_limit': 128000000 - size,
          'guard_files': 9000, 'guard_bytes': 120 * 1048576,
          'ok': len(files) < 9000 and size < min(120 * 1048576, 128000000)}
print(json.dumps(result, indent=2))
sys.exit(0 if result['ok'] else 1)
