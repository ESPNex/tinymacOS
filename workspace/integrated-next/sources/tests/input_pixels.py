"""Pixel-level cursor assertion against the actual shipped arrow, not guest state."""
import time
import numpy as np
from guest import ROOT
sprite=np.frombuffer((ROOT/'assets/symbols.rgba').read_bytes(),dtype=np.uint8).reshape(-1,64,64,4)[15]
def cursor(g,x,y):
    g.move(70,75);bg=np.asarray(g.snap(),dtype=np.int32);n=26;ix=np.arange(n)*64//n
    p=sprite[ix[:,None],ix].astype(np.int32);a=p[:,:,3:4];mask=a[:,:,0]>=200
    region=bg[y-2:y-2+n,x-3:x-3+n]
    expect=np.where(a==255,p[:,:,:3],(region*(256-a)+p[:,:,:3]*a)//256)
    g.move(x,y);end=time.monotonic()+3
    while True:
        actual=np.asarray(g.snap(),dtype=np.int32)[y-2:y-2+n,x-3:x-3+n]
        if np.max(np.abs(actual-expect)[mask])<=2 and np.max(np.abs(actual-region)[mask])>=3:return
        assert time.monotonic()<end,(g.tag,x,y,'cursor sprite mismatch')
        time.sleep(.05)
