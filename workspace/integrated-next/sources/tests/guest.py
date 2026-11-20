"""Black-box OVMF/QEMU driver. Input uses the configured virtual devices, with COM1 observation and GOP captures."""
import json, os, re, socket, struct, subprocess, time
from pathlib import Path
from PIL import Image
ROOT = Path(__file__).resolve().parents[1]
KEYS = {' ': 'spc', '/':'shift-7', '.':'dot', ',':'comma', '-':'slash', '_':'shift-slash', '+':'bracket_right', '=':'shift-0', '*':'shift-bracket_right', ':':'shift-dot', '"':'shift-2', '\n':'ret',
 '|':'shift-grave_accent','>':'shift-less','<':'less',"'":'minus','~':'alt_r-equal','$':'shift-4','@':'alt_r-semicolon','#':'alt_r-apostrophe','%':'shift-5','?':'shift-minus',';':'shift-comma','(':'shift-8',')':'shift-9','!':'shift-1','\\':'grave_accent',
 '[':'alt_r-bracket_left',']':'alt_r-bracket_right','{':'alt_r-shift-bracket_left','}':'alt_r-shift-bracket_right','&':'shift-6','`':'alt_r-minus','£':'shift-3','€':'alt_r-e','à':'apostrophe','è':'bracket_left','é':'shift-bracket_left','ì':'equal','ò':'semicolon','ù':'backslash',
 }

def default_iso():
    out=Path(os.environ.get('TMC_BUILD',Path(os.environ.get('TMPDIR','/tmp'))/f'tinymacos-{os.getuid()}'/'1.3'))
    return out/'tinymacOS-1.3-x86_64.iso'

class Guest:
    def __init__(self, iso, work, evidence, tag, disk='persistent', live=False, capture_boot=False, boot_disk=False, launcher=None):
        self.work, self.ev, self.tag = work, evidence, tag
        self.log, self.sock = work/(tag+'.log'), work/(tag+'.qmp')
        self.pos = 0
        env = {**os.environ, 'STATE':str(work/disk), 'LIVE':str(int(live)), 'ISO':str(iso),'BOOT_DISK':str(int(boot_disk))}
        # Seed the *virtual* RTC, not the guest's clock API. QEMU advances it.
        self.p = subprocess.Popen([str(launcher or ROOT/'tools/run-qemu.sh'), '-rtc', 'base=2026-11-18T10:30:00,clock=vm', '-display','none','-serial','file:'+str(self.log),'-qmp','unix:'+str(self.sock)+',server=on,wait=off'], cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        try:
            # Connect before firmware hands off so real splash frames can be sampled.
            until=time.monotonic()+15
            while not self.sock.exists() and time.monotonic()<until:
                if self.p.poll() is not None:raise RuntimeError(self.p.stdout.read().decode())
                time.sleep(.025)
            self.s=socket.socket(socket.AF_UNIX);self.s.settimeout(8)
            # QEMU can create its Unix socket just before listen(). Retry the
            # connection race, not guest assertions or failed OS boots.
            while True:
                try:self.s.connect(str(self.sock));break
                except (ConnectionRefusedError,FileNotFoundError):
                    if self.p.poll() is not None or time.monotonic()>=until:raise
                    time.sleep(.025)
            self.f=self.s.makefile('rwb',buffering=0);self.f.readline();self.req('qmp_capabilities')
            self.reset_pointer();self.boot_samples=[]
            if capture_boot:self.capture_stages()
            self.wait('[gop] first userspace frame presented',timeout=55,mark=0)
            time.sleep(.18)
        except Exception:
            self.close();raise
    def capture_stages(self):
        until=time.monotonic()+48;prev=-1;frame=self.work/'boot.ppm'
        while '[gop] first userspace frame presented' not in self.text() and time.monotonic()<until:
            self.req('screendump',filename=str(frame))
            with Image.open(frame) as im:
                # Ignore the initial GOP clear/scanout: the first pixel of the logo
                # can arrive before firmware finishes the silhouette and draws the track.
                if (im.size==(1280,800) and im.getpixel((640,378))[:3]==(255,255,255) and
                    im.getpixel((40,40))[:3]==(0,0,0) and im.getpixel((730,492))[:3] in ((52,52,52),(238,238,238))):
                    filled=sum(im.getpixel((x,492))[0]>180 for x in range(544,736))
                    if filled!=prev:
                        self.boot_samples.append((filled,time.monotonic(),im.copy()));prev=filled
            time.sleep(.025)
    def enroll(self,name='Elena',password='local-test-285'):
        self.type(name);self.key('tab');self.type(password);self.key('tab');self.type(password);m=self.mark();self.key('ret');self.wait('[session] authenticated',mark=m,timeout=30);self.wait('motion open frames=',mark=m)
    def unlock(self,password='local-test-285',fresh=True):
        self.wait('[user:Scrivania] login ready',timeout=30,mark=0)
        # On disk boots the session's snapshot/restore churn hammers AHCI right
        # after login; QEMU's main loop then starves USB IN servicing and keys
        # typed into that window are lost (the login 's' flood). Wait for the
        # volume-status line that is printed after the churn, then settle.
        deadline=time.time()+12
        while time.time()<deadline:
            if '[fs] volume encrypted; user files=' in self.text():break
            time.sleep(.25)
        time.sleep(.6)
        self.type(password);m=self.mark();self.key('ret');self.wait('[session] authenticated',mark=m,timeout=30);self.wait('desktop unlocked',mark=m)
        if fresh:self.wait('motion open frames=',mark=m)
    def reset_pointer(self):self.mx,self.my=1280-92,800-131
    def __enter__(self):return self
    def __exit__(self,T,e,tb):
        if e and hasattr(self,'f') and self.p.poll() is None:
            try:self.shot('failure-'+self.tag)
            except Exception:pass
        self.close()
    def text(self):return self.log.read_text(errors='replace') if self.log.exists() else ''
    def mark(self):return len(self.text())
    def wait(self,pattern,timeout=10,mark=None):
        beg=self.pos if mark is None else mark;end=time.monotonic()+timeout
        while time.monotonic()<end:
            s=self.text();idx=s.find(pattern,beg)
            if idx>=0:self.pos=idx+len(pattern);return
            if self.p.poll() is not None:raise RuntimeError(f'{self.tag} exited: {self.p.stdout.read()!r}\n'+s[-2300:])
            if 'PANIC:' in s[beg:] or '[exception]' in s[beg:]:raise RuntimeError(s[-2500:])
            time.sleep(.015)
        raise AssertionError(f'{self.tag}: missing {pattern!r}\n'+self.text()[-2600:])
    def req(self,cmd,**args):
        self.f.write(json.dumps({'execute':cmd,'arguments':args}).encode()+b'\n')
        while True:
            r=json.loads(self.f.readline());
            if 'error' in r:raise RuntimeError(r)
            if 'return' in r:return r['return']
    def hmp(self,c):return self.req('human-monitor-command',**{'command-line':c})
    def key(self,k,dt=.12,hold=.04):
        # input-send-event, never send-key/HMP sendkey: those park events on
        # QEMU's kbd_queue behind a VIRTUAL-clock delay item; once that queue
        # wedges, later keys are silently dropped and a held key autorepeats
        # forever (the login 's' flood). Direct press -> gap -> release keeps
        # the queue untouched and every key visible to the guest's USB poll.
        parts=[q for q in k.split('-') if q]
        down=[{'type':'key','data':{'down':True,'key':{'type':'qcode','data':q}}} for q in parts]
        up=[{'type':'key','data':{'down':False,'key':{'type':'qcode','data':q}}} for q in reversed(parts)]
        self.req('input-send-event',events=down)
        if hold:time.sleep(hold)
        try:
            self.req('input-send-event',events=up)
        except (BrokenPipeError, OSError, RuntimeError, json.JSONDecodeError):
            pass  # VM powered off between press and release (install S5)
        if dt:time.sleep(dt)
    def type(self,chars):
        for c in chars:
            t0=time.time()
            self.key(KEYS.get(c,('shift-'+c.lower() if c.isupper() else c)))
            print('type %r dt=%.3f' % (c, time.time()-t0), flush=True)
    def cmd(self,s,expect):
        m=self.mark();self.type(s);self.key('ret');self.wait(expect,mark=m)
    def move(self,x,y):
        if os.environ.get('TMC_POINTER','tablet')=='mouse':
            # Relative profile: HMP mouse_move on usb-mouse.
            while (self.mx,self.my)!=(x,y):
                dx=max(-46,min(46,x-self.mx));dy=max(-46,min(46,y-self.my));self.hmp(f'mouse_move {dx} {dy}');self.mx+=dx;self.my+=dy;time.sleep(.024)
            time.sleep(.095)
            return
        # Default tablet profile needs absolute axis events (relative HMP drifts).
        w,h=1280,800
        assert 0<=x<w and 0<=y<h
        self.req('input-send-event',events=[{'type':'abs','data':{'axis':axis,'value':round(v*32767/(n-1))}} for axis,v,n in [('x',x,w),('y',y,h)]])
        self.mx,self.my=x,y;time.sleep(.09)
    def mouse(self,down):self.hmp('mouse_button '+('1' if down else '0'));time.sleep(.11)
    def click(self,x,y):self.move(x,y);self.mouse(True);self.mouse(False)
    def drag(self,x,y,dx,dy):self.move(x,y);self.mouse(True);self.move(x+dx,y+dy);self.mouse(False)
    def bounds(self,app):
        return tuple(map(int,re.findall(r'\[user:Scrivania\] view '+str(app)+r' (\d+) (\d+) (\d+) (\d+)',self.text())[-1]))
    def frame(self):
        p=self.work/(self.tag+'-capture.ppm');self.req('screendump',filename=str(p));
        with Image.open(p) as im:
            assert im.size==(1280,800),im.size
            return im.convert('RGB')
    def shot(self,name):
        time.sleep(.31);im=self.frame();im.save(self.ev/(name+'.png'));return im
    def spawn(self,key,name):
        m=self.mark();self.key(key);self.wait('[window] open '+name,mark=m);self.wait('motion open frames=',mark=m)
    def shutdown(self):
        self.cmd('off','[power] ACPI S5');self.p.wait(timeout=9);assert self.p.returncode==0
    def close(self):
        if self.p.poll() is None:self.p.terminate();self.p.wait(timeout=8)
        if self.log.exists():(self.ev/(self.tag+'-serial.txt')).write_text(re.sub(r'\x1b\[[\d;=?]*[A-Za-z]','',self.text()))
        if hasattr(self,'f'):self.f.close()
        if hasattr(self,'s'):self.s.close()
        if self.p.stdout:self.p.stdout.close()

def audit_bytes(raw):
    """Independent TMCBFS1/TinyFS v1 reader; never writes the disk."""
    magic = raw[:8]
    if magic == b'TMCBFS1\0':
        rev,n,base,ready=struct.unpack_from('<IIII',raw,8)
        assert (rev,n,base,ready)==(2,384,144,1)
        node_sec = 64                      # btrfs node table (NODE0)
    elif magic == b'TINYFS1\0':
        version,n,base,ready=struct.unpack_from('<IIII',raw,8)
        assert (version,n,base,ready)==(1,96,16,1)
        node_sec = 1                       # legacy v1 layout
    else:
        raise AssertionError('unknown volume magic %r' % magic)
    nodes=[struct.unpack_from('<BBHII48sI',raw,node_sec*512+i*64) for i in range(n)]  # 8 nodes/sector
    def full(i):return (full(nodes[i][2]).rstrip('/')+'/'+nodes[i][5].split(b'\0')[0].decode()) if i else '/'
    return {full(i):(None if k==1 else raw[(base+i*16)*512:(base+i*16)*512+z]) for i,(k,ro,p,z,g,name,r) in enumerate(nodes) if k}
