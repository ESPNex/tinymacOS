"""Native input smoke with actual emulated PS/2 or USB controller, not a stub.
Env TMC_TEST_INPUT=ps2|ehci|uhci|companion; TMC_INPUT_LAUNCHER, TMC_TEST_ISO.
"""
import json,os,tempfile,time
from pathlib import Path
from retail_guest import Retail
from guest import ROOT
import numpy as np
w=Path(tempfile.mkdtemp(prefix='tmc-input2-'+os.environ.get('TMC_TEST_INPUT','ps2')+'-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TMC_POINTER='mouse',TOOLROOT='/var/tmp/tmc/tools/root')
iso=Path(os.environ['TMC_TEST_ISO']);launcher=Path(os.environ.get('TMC_INPUT_LAUNCHER',str(ROOT/'tools/run-input-qa.sh')))
sprite=np.frombuffer((ROOT/'assets/symbols.rgba').read_bytes(),dtype=np.uint8).reshape(-1,64,64,4)[15]
def cursor(g,x,y):
 g.move(70,75);bg=np.asarray(g.snap(),dtype=np.int32);n=26;ix=np.arange(n)*64//n;p=sprite[ix[:,None],ix].astype(np.int32);a=p[:,:,3:4];mask=a[:,:,0]>=200
 region=bg[y-2:y-2+n,x-3:x-3+n];expect=np.where(a==255,p[:,:,:3],(region*(256-a)+p[:,:,:3]*a)//256)
 g.move(x,y);end=time.monotonic()+3
 while True:
  actual=np.asarray(g.snap(),dtype=np.int32)[y-2:y-2+n,x-3:x-3+n]
  if np.max(np.abs(actual-expect)[mask])<=2 and np.max(np.abs(actual-region)[mask])>=3:return
  assert time.monotonic()<end,(x,y,'cursor mismatch')
  time.sleep(.05)
record={'passed':False,'profile':os.environ.get('TMC_TEST_INPUT','ps2'),'work':str(w),'physical_hardware_tested':False}
try:
 with Retail(iso,w,ev,'input',live=True,launcher=launcher) as g:
  record['usb']=g.hmp('info usb');record['mice']=g.req('query-mice');print(record,flush=True)
  assert 'USB Keyboard' not in record['usb'],'PS/2 keyboard must be the only keyboard'
  cursor(g,762,482);cursor(g,142,613);g.shot('setup-cursor');print('PASS cursor at setup',flush=True)
  for y,t in [(448,'NativeInput'),(489,'local-test-285'),(530,'local-test-285')]:g.click(641,y);g.type(t)
  g.click(640,575);g.visible('Preferiti',40);g.shot('desktop');print('PASS PS2 typing + mouse click account + desktop',flush=True)
  g.key('f2');g.visible('Sessione locale');g.cmd('echo PS2_INPUT_OK','PS2_INPUT_OK');g.shot('terminal');record['passed']=True
finally:(w/'result.json').write_text(json.dumps(record,indent=2)+'\n');print(w,flush=True)
