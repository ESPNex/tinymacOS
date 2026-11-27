"""Read-only QMP observers must use the real compiled ABI, never stale offsets."""
import json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def process_layout():
 with tempfile.TemporaryDirectory(prefix='tmc-layout-',dir='/var/tmp') as d:
  d=Path(d);src=d/'layout.c';exe=d/'layout'
  src.write_text('#include <stdio.h>\n#include <stddef.h>\n#include "kernel/k.h"\nint main(void){printf("[%zu,%zu,%zu,%zu,%zu,%zu,%d]\\n",sizeof(Process),offsetof(Process,app),offsetof(Process,cpu_ticks),offsetof(Process,win),sizeof(Win),offsetof(Win,seq),NPROC);}\n')
  subprocess.run(['gcc','-I'+str(ROOT),'-I'+str(ROOT/'include'),'-I'+str(ROOT/'arch'),str(src),'-o',str(exe)],check=True)
  return json.loads(subprocess.check_output([str(exe)],text=True))
