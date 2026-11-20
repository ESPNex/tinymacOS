"""Debug-only protocol census in an external disposable modified disk."""
import os,sys,time
from pathlib import Path
sys.path.insert(0,'/home/user/tinymacOS/tests')
from retail_guest import Retail
w=Path('/var/tmp/tmc-bios-debug-fixture');(w/'evidence').mkdir(exist_ok=True)
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',DISK=str(w/'system.raw'),TMC_REC_DEBUG='1',TMC_POINTER='mouse')
# Force recovery by damaging ONLY this disposable debug fixture's kernel.
cache=Path('/var/tmp/tmc-bios-debug-system/KernelCache').read_bytes();data=(w/'system.raw').read_bytes();where=data.index(cache)+128
with (w/'system.raw').open('r+b') as f:f.seek(where);b=f.read(1);f.seek(-1,1);f.write(bytes([b[0]^8]))
class Rec(Retail):initial='Scegli come proseguire.'
with Rec('/not-attached',w,w/'evidence','debug',boot_disk=True,launcher=Path('/home/user/tinymacOS/tools/run-bios-installed.sh')) as g:
 print((w/'persistent/recovery-debug.log').read_text(errors='replace'))
 for _ in range(3):g.key('down',.3,hold=.15)
 g.shot('keys');g.move(400,300);g.click(1060,648);g.shot('mouse')
 print((w/'persistent/recovery-debug.log').read_text(errors='replace'))
