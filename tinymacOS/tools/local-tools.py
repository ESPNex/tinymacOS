#!/usr/bin/env python3
"""Install only the locked, SHA-256-checked Debian amd64 packages under a user prefix.
Requires a compatible Debian 13 amd64 base with GCC, binutils, Python 3, dpkg-deb.
No apt install, root, package maintainer scripts, mounts or loop devices are used.
"""
from pathlib import Path
import sys,json,hashlib,urllib.request,subprocess,concurrent.futures
lock=Path(__file__).with_name('debian-amd64.lock.json'); manifest=json.loads(lock.read_text()); target=Path(sys.argv[1] if len(sys.argv)>1 else '~/.local/tinymacos-tools').expanduser().resolve();target.mkdir(parents=True,exist_ok=True);cache=target/'packages';cache.mkdir(exist_ok=True)
def fetch(it):
 p=cache/it['name'];
 if p.exists() and hashlib.sha256(p.read_bytes()).hexdigest()==it['sha256']: return p
 with urllib.request.urlopen(it['url'],timeout=45) as r: data=r.read()
 if len(data)!=it['size'] or hashlib.sha256(data).hexdigest()!=it['sha256']:raise ValueError('Package verification failed: '+it['name'])
 p.write_bytes(data);return p
with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool: files=list(pool.map(fetch,manifest))
for p in files:subprocess.run(['dpkg-deb','-x',p,target],check=True)
print('Ready. Set TOOLROOT='+str(target)+' before building or running.');print('Headless QEMU works with this prefix; use -display vnc=127.0.0.1:0 or install your usual QEMU GUI frontend for an interactive display.')
