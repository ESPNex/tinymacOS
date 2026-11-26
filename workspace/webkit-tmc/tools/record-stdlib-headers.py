#!/usr/bin/env python3
"""Record actual GCC/libstdc++/C header closure, not a claim of target libc ABI.
These are compile-time host headers only; target links zero host libraries.
"""
from pathlib import Path
import sys,json,hashlib,subprocess
build,out=map(Path,sys.argv[1:3]);files=set()
for p in build.glob('stdlib-*.d'):
 for name in p.read_text().split(':',1)[1].replace('\\\n',' ').split():
  q=Path(name)
  if q.is_file() and (str(q).startswith('/usr/include/') or str(q).startswith('/usr/lib/gcc/')):files.add(q)
assert files,'No header dependency closure found'
record={'compiler':subprocess.check_output(['g++','--version'],text=True).splitlines()[0],'kind':'actual build-host header closure for selected templates only','hosted_header_configuration':True,'host_cpp_binary_library_linked':False,'full_libstdcpp_or_libcpp_port':False,'headers':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(files)}}
out.write_text(json.dumps(record,indent=2)+'\n');print('Recorded',len(files),'actual standard/C header inputs; no C++ binary library implied')
