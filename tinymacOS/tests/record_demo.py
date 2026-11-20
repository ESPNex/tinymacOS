#!/usr/bin/env python3
"""Record the actual boot, returning login and Finder's boot volume inspector. No render overlays.
Uses disposable disks, QMP/USB and real GOP frames. Optional HOST deps: Pillow, numpy, OpenCV VP8.
"""
import argparse, io, os, tempfile, threading, time
from pathlib import Path
import cv2,numpy as np
from PIL import Image
from guest import ROOT,Guest,default_iso
from orizzonte import install
ap=argparse.ArgumentParser();ap.add_argument('output',type=Path);args=ap.parse_args();ISO=Path(os.environ.get('ISO',default_iso())).resolve();pw='sample-user-928';fps=20;size=(1024,640)
cv2.setNumThreads(1);samples=[];fail=[];lock=threading.RLock();stop=threading.Event()
def store(t,im):
 b=io.BytesIO();im.resize(size,Image.Resampling.LANCZOS).save(b,format='JPEG',quality=93,subsampling=0);samples.append((t,b.getvalue()))
with tempfile.TemporaryDirectory(prefix='aurora-record-') as tmp:
 w=Path(tmp)
 with Guest(ISO,w,w,'enroll') as g:
    g.enroll('Elena',pw);install(g)
 with Guest(None,w,w,'record',capture_boot=True,boot_disk=True) as g:
    assert len(g.boot_samples)>=4
    base=g.boot_samples[0][1];
    for _,t,im in g.boot_samples:store(t-base,im)
    g.boot_samples=[];req=g.req
    def guarded(cmd,**a):
        with lock:return req(cmd,**a)
    g.req=guarded
    def grab():
        try:
            while not stop.is_set():
                t=time.monotonic();store(t-base,g.frame());stop.wait(max(.001,.048-(time.monotonic()-t)))
        except Exception as e:fail.append(e)
    th=threading.Thread(target=grab);th.start()
    try:
        time.sleep(1.3);g.unlock(pw);time.sleep(.7);X,Y,_,_=g.bounds(1);g.click(X+80,Y+43+292);g.wait('[user:Finder] /System/Kernel',mark=0);time.sleep(1.0)
        g.click(X+302,Y+43+117);g.key('spc');g.wait('verified bytes=',mark=0,timeout=30);time.sleep(1.55);g.key('right');time.sleep(.55);g.key('end');time.sleep(.7);g.key('esc');time.sleep(.6);m=g.mark();g.key('ctrl-shift-q');g.wait('login ready',mark=m);time.sleep(1.0)
    finally:stop.set();th.join(timeout=8)
    if fail:raise fail[0]
    assert not th.is_alive() and len(samples)>80
    # End recording first; field input is never echoed to the host log.
    g.click(682,743);g.key('ret');g.wait('[power] ACPI S5',mark=0);g.p.wait(timeout=9)
 out=args.output.resolve();out.parent.mkdir(parents=True,exist_ok=True);enc=cv2.VideoWriter(str(out),cv2.VideoWriter_fourcc(*'VP80'),fps,size);assert enc.isOpened()
 i=0;last=-1;count=round(samples[-1][0]*fps)+1
 for j in range(count):
    while i+1<len(samples) and samples[i+1][0]<=j/fps:i+=1
    if i!=last:frame=cv2.imdecode(np.frombuffer(samples[i][1],np.uint8),cv2.IMREAD_COLOR);last=i
    enc.write(frame)
 enc.release();vid=cv2.VideoCapture(str(out));n=0
 while True:
    ok,frame=vid.read()
    if not ok:break
    assert frame.shape[:2]==(size[1],size[0]);n+=1
 vid.release();assert n==count;print(f'{out.name}: {n} decoded VP8 frames, {n/fps:.2f} s; {len(samples)} native captures; {out.stat().st_size} bytes')
