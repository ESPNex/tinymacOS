"""Real UI and kernel-window observation with 18 open native windows.
Read-only QMP memory observations, no injected guest actions or test syscall.
"""
from pathlib import Path
import os,sys,time,struct,json,hashlib,subprocess,tempfile
from retail_guest import Retail
from kernel_layout import process_layout
from input_pixels import cursor
build=Path(os.environ['TMC_BUILD']);iso=Path(os.environ['TMC_STAGE_ISO']);qa=Path(os.environ['TMC_QA_DIR'])
label=os.environ.get('TMC_PERF_LABEL','candidate');baseline=os.environ.get('TMC_PERF_BASELINE')=='1'
w=Path(tempfile.mkdtemp(prefix='tmc-many-windows-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='mouse',OCR_ROOT=os.environ['TOOLROOT'])
symbols={x.split()[-1]:int(x.split()[0],16) for x in subprocess.check_output(['nm','--defined-only',str(build/'kernel.debug')],text=True).splitlines() if len(x.split())>=3}
# Compile the current ABI; descriptor changes must not silently break observers.
stride,appoff,cpuoff,winoff,winbytes,seqoff,nproc=process_layout()
r={'passed':False,'label':label,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'physical_hardware_tested':False,'process_abi':[stride,appoff,cpuoff,winoff,winbytes,seqoff]}
try:
 with Retail(iso,w,ev,'many-windows',live=True) as g:
  def memory(name,n):
   p=w/'observation.bin';g.req('pmemsave',val=symbols[name],size=n,filename=str(p));return p.read_bytes()
  def processes():
   b=memory('proc',stride*nproc);rows=[]
   for i in range(nproc):
    at=i*stride;pid,app=struct.unpack_from('<ii',b,at)
    if pid:
     rows.append({'pid':pid,'app':app,'cpu':struct.unpack_from('<Q',b,at+cpuoff)[0],'window':struct.unpack_from('<i',b,at+winoff)[0],'seq':struct.unpack_from('<I',b,at+winoff+seqoff)[0]})
   return rows
  def desktop_cpu():return next(p['cpu'] for p in processes() if p['app']==0)
  g.enroll('Finestre');g.key('f2');g.visible('Sessione locale')
  for i in range(16):
   g.key('f2');g.cmd('open -a Calcolatrice');time.sleep(.35)
  deadline=time.monotonic()+50
  while time.monotonic()<deadline:
   rows=processes();calcs=[p for p in rows if p['app']==6 and p['window']]
   if len(calcs)==16:break
   time.sleep(.2)
  assert len(calcs)==16,rows
  r['open_windows']=sum(bool(p['window']) for p in rows);assert r['open_windows']>=18
  time.sleep(3)
  r['before_noop_keys']=before=processes()
  if not baseline:r['draw_requests_before'],r['draw_unchanged_before']=struct.unpack('<Q',memory('tmc_window_draw_requests',8))[0],struct.unpack('<Q',memory('tmc_window_draw_unchanged',8))[0]
  # Unknown calculator key: app receives real USB keys, republishes unchanged
  # draw list. No document/calculation state changes are requested.
  for i in range(12):g.key('a',dt=.08)
  time.sleep(2);r['after_noop_keys']=after=processes()
  changed=[p['pid'] for p in after if p['app']==6 and any(a['pid']==p['pid'] and a['seq']!=p['seq'] for a in before)]
  r['calculator_sequences_changed_after_noop']=changed
  if not baseline:
   r['draw_requests_after']=struct.unpack('<Q',memory('tmc_window_draw_requests',8))[0]
   r['draw_unchanged_after']=struct.unpack('<Q',memory('tmc_window_draw_unchanged',8))[0]
   assert not changed,changed
   assert r['draw_unchanged_after']>r['draw_unchanged_before']
  else:assert changed,'negative baseline must actually invalidate identical calculator frames'
  a=desktop_cpu();start=time.monotonic();time.sleep(12);r['idle_sample_wall_seconds']=time.monotonic()-start;r['idle_desktop_scheduled_ticks']=desktop_cpu()-a
  cursor(g,1100,690);cursor(g,220,240)
  g.key('f2');g.visible('Sessione locale');g.cmd('echo MANY_WINDOWS_RESPONSIVE','MANY_WINDOWS_RESPONSIVE');g.shot('many-windows-responsive')
  r['mouse_and_Terminal_responsive']=True;r['passed']=True
finally:
 (qa/('many-windows-'+label+'.json')).write_text(json.dumps(r,indent=2)+'\n')
 for p in w.glob('*.ppm'):p.unlink()
 (w/'observation.bin').unlink(missing_ok=True)
print('PASS many native windows',label,r['open_windows'],'desktop scheduled ticks',r['idle_desktop_scheduled_ticks'])
