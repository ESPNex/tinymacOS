"""QEMU black-box retail UI driver: no instrumentation or diagnostic service in the guest."""
import os,re,subprocess,time,unicodedata
from pathlib import Path
from PIL import Image
from pixels import locate
from guest import Guest,KEYS
# Never attach a user's disk or install media inherited from an interactive launcher.
# Every retail test explicitly assigns its own disposable fixtures after importing this module.
for _key in ('ISO','DISK','LEGACY_DATA','RECOVERY_IMG','STATE','BOOT_DISK','LIVE'):
    os.environ.pop(_key,None)
def fold(s):return ''.join(c for c in unicodedata.normalize('NFD',s.lower()) if c.isalnum())
class Retail(Guest):
    initial='account'
    def ocr(self,box=None,psm=11):
        p=self.work/(self.tag+'-ocr.ppm');self.req('screendump',filename=str(p))
        if box:
            with Image.open(p) as im:
                r=im.crop(box);r=r.resize((r.width*2,r.height*2));r.save(p)
        root=os.environ.get('OCR_ROOT');env={**os.environ,'OMP_THREAD_LIMIT':'1'}
        if root:env.update(LD_LIBRARY_PATH=root+'/usr/lib/x86_64-linux-gnu',TESSDATA_PREFIX=root+'/usr/share/tesseract-ocr/5/tessdata')
        return subprocess.run([(root+'/usr/bin/' if root else '')+'tesseract',str(p),'stdout','-l','eng','--psm',str(psm)],stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,env=env,text=True,check=True).stdout
    def move(self,x,y):
        if os.environ.get('TMC_POINTER','tablet')=='mouse':return super().move(x,y)
        w,h=getattr(self,'display_size',(1280,800))
        assert 0<=x<w and 0<=y<h
        self.req('input-send-event',events=[{'type':'abs','data':{'axis':axis,'value':round(v*32767/(n-1))}} for axis,v,n in [('x',x,w),('y',y,h)]])
        self.mx,self.my=x,y;time.sleep(.09)
    def snap(self):
        p=self.work/(self.tag+'-snap.ppm');self.req('screendump',filename=str(p))
        with Image.open(p) as im:
            im=im.convert('RGB')
            self.display_size=im.size   # abs input must track runtime reswitch
            return im
    def visible(self,t,timeout=24,box=None,faces=(0,1,2,3,5,6,8)):
        init=time.monotonic();qs=t if isinstance(t,(tuple,list)) else [t]
        while time.monotonic()-init<timeout:
            if self.p.poll() is not None:raise RuntimeError(self.p.stdout.read())
            im=self.snap()
            if box:im=im.crop(box)
            for q in qs:
                pos=locate(im,q,faces)
                if pos:return (pos[1][0]+(box[0] if box else 0),pos[1][1]+(box[1] if box else 0))
            time.sleep(.22)
        raise AssertionError(f'{self.tag} expected {qs!r}; visible text: '+self.ocr(box))
    def wait(self,*a,**k):self.visible(self.initial,52)
    def shot(self,name):return super().shot(name)
    def enroll(self,name='Elena',password='local-test-285'):
        self.type(name);self.key('tab');self.type(password);self.key('tab');self.type(password);self.key('ret');self.visible('Preferiti',34);time.sleep(.65)
    def unlock(self,password='local-test-285'):
        self.type(password);self.key('ret');self.visible('Preferiti',34);time.sleep(.65)
    def cmd(self,s,expect=None):self.type(s);self.key('ret',.2);return self.visible(expect) if expect else None
    def app(self,key,s):self.key(key);self.visible(s);time.sleep(.48)
