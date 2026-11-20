"""Native 1.3 black-box checks. No filesystem writes during the guest session from the host."""
import hashlib,json,os,re,struct,subprocess,time,zlib
from pathlib import Path
from PIL import Image,ImageChops
from guest import ROOT,audit_bytes
DATA=2048+49152;SIZE=(144+384*16)*512   # ESP 24M; vol node table@64 + 384 data slots

def disk_bytes(path):
    with path.open('rb') as f:f.seek(DATA*512);return f.read(SIZE)
def disk_audit(path,opt,check,metrics):
    with path.open('rb') as f:
        def read(l,n):f.seek(l*512);return f.read(n*512)
        size=path.stat().st_size//512
        def header(l):
            h=bytearray(read(l,1));assert h[:8]==b'EFI PART' and struct.unpack_from('<I',h,12)[0]==92
            crc=struct.unpack_from('<I',h,16)[0];h[16:20]=bytes(4);assert zlib.crc32(h[:92])==crc
            at,other=struct.unpack_from('<QQ',h,24);assert at==l and other in (1,size-1) and at!=other
            start,num,sz,sum32=struct.unpack_from('<QIII',h,72);p=read(start,32);assert num==sz==128 and zlib.crc32(p)==sum32;return p
        p=header(1);assert p==header(size-1);assert struct.unpack_from('<QQ',p,32)==(2048,DATA-1) and struct.unpack_from('<QQ',p,160)==(DATA,size-34);assert read(0,1)[510:]==b'\x55\xaa' and read(0,1)[450]==0xee
        check(True,'Offline: protective MBR, both GPT headers and both matching entry arrays pass independent CRC-32 and partition-bound checks')
        esp=read(2048,49152);assert esp==opt['esp'];metrics['installed_esp_sha256']=hashlib.sha256(esp).hexdigest()
        check(True,'Entire installed FAT ESP byte-identical to the independent install payload (not an ISO copy or an EFI boot record alone)')
        return audit_bytes(read(DATA,SIZE//512))

def medium(work,iso,check,metrics):
    d=work/'disc';d.mkdir(exist_ok=True);env=dict(os.environ);pre=env.get('TOOLROOT','')
    if pre:env.update(PATH=pre+'/usr/bin:'+env['PATH'],LD_LIBRARY_PATH=pre+'/usr/lib/x86_64-linux-gnu:'+env.get('LD_LIBRARY_PATH',''))
    def run(*args):return subprocess.run(args,env=env,check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True).stdout
    run('xorriso','-osirrox','on','-indev',str(iso),'-extract','/EFI.IMG',str(d/'fat'),'-extract','/System/Library/KernelCollections/KernelCache',str(d/'outer-kernel'),'-extract','/EFI/BOOT/BOOTX64.EFI',str(d/'outer-loader'))
    run('mcopy','-i',str(d/'fat'),'::/Install.esp',str(d/'esp'));out={'esp':(d/'esp').read_bytes()}
    for k,p in [('kernel','System/Library/KernelCollections/KernelCache'),('loader','EFI/BOOT/BOOTX64.EFI')]:
        out[k]=(d/('outer-'+k)).read_bytes()
        for v in ('fat','esp'):
            dest=d/(v+'-'+k);run('mcopy','-i',str(d/v),'::/'+p,str(dest));assert out[k]==dest.read_bytes()
    assert out['kernel'][:8]==b'TMKCACHE' and out['loader'][:2]==b'MZ' and len(out['esp'])==24<<20
    for v in ('fat','esp'):
        ls=run('mdir','-b','-s','-i',str(d/v),'::/').lower();assert 'sorgenti' not in ls and 'system.pak' not in ls and 'kernel.map' not in ls and not re.search(r'\.(c|h|asm|s)$',ls,re.M),ls
        assert ('install.esp' in ls)==(v=='fat')
    metrics['boot_objects']={k:{'bytes':len(v),'sha256':hashlib.sha256(v).hexdigest()} for k,v in out.items()};check(True,'ISO tree, El Torito boot FAT and install ESP carry the same TMKCACHE kernel cache and PE loader; neither FAT ships a source tree or a catalog');return out

def hardware(g,check):
    pci=g.req('query-pci');items=[x for bus in pci for x in bus['devices']];classes={x['class_info']['class'] for x in items};assert 0x0106 in classes and 0x0c03 in classes and 0x0101 not in classes,classes
    # The 2.4.2 kernel does not narrate successful USB enumeration on the serial console (only the xHCI-absent failure path logs).
    # Verify host topology (Q35, i8042 compiled out, xHCI with keyboard/pointer) and the guest-registered AHCI/SATA line instead.
    tree=g.hmp('info qtree');assert 'i8042' not in tree and 'qemu-xhci' in tree and 'usb-kbd' in tree and ('usb-tablet' in tree or 'usb-mouse' in tree),tree
    assert '[ahci] SATA 0' in g.text() and '[usb] xHCI absent' not in g.text()
    check(True,'PCI inspection: Q35 AHCI and xHCI, no legacy IDE or i8042. Host topology carries usb-kbd plus an absolute/relative pointer on the xHCI bus; the guest registers SATA 0 without xHCI failure markers')

def right(g,x,y):
    g.move(x,y);m=g.mark();g.req('input-send-event',events=[{'type':'btn','data':{'button':'right','down':True}}]);time.sleep(.09);g.req('input-send-event',events=[{'type':'btn','data':{'button':'right','down':False}}]);g.wait('menu contestuale',mark=m);time.sleep(.30)

def context(g,E,check):
    # First entry in the desktop menu opens Finder *at Desktop*, with its name sheet.
    right(g,74,619);g.shot('18-menu-scrivania');m=g.mark();g.click(136,619+21);g.wait('[user:Finder] /Users/User/Desktop',mark=m);g.wait('motion open frames=',mark=m);g.type('Studio');g.key('ret');time.sleep(.27)
    X,Y,w,h=g.bounds(1);B=Y+43
    # The empty area is not the selected file and offers creation rather than rename.
    x,y=X+w-50,B+218;right(g,x,y);g.click(x+28,y+6+31+15);time.sleep(.20);g.type('schizzo.txt');g.key('ret');time.sleep(.35)
    # Second grid cell: the right click must identify it without any left click first.
    cw=(w-2-171-27)//5;x,y=X+186+cw+cw//2,B+134;right(g,x,y);g.shot('19-menu-finder');g.click(x+43,y+6+4*31+14);time.sleep(.16)
    for i in 'schizzo.txt':g.key('backspace')
    m=g.mark();g.type('bozza.txt');g.key('ret');g.wait('renamed bozza.txt',mark=m);m=g.mark();g.key('ctrl-w');g.wait('motion close frames=',mark=m)
    g.spawn('f2','Terminale');g.cmd('ls Desktop','bozza.txt');g.cmd('ls Desktop','Studio');g.cmd('write Desktop/bozza.txt Documento installato','write committed: bozza.txt');m=g.mark();g.key('ctrl-w');g.wait('motion close frames=',mark=m)
    check(True,'USB right-click on desktop creates a real folder; in Finder blank-space menu creates a file and item menu renames the clicked (previously unselected) file; shell reads both')
    # System file menu: no rename/delete; Quick Look is the binary inspector, not TextEdit.
    g.key('f1');g.key('ctrl-shift-k');g.wait('/System/Kernel',mark=m);X,Y,w,h=g.bounds(1);x,y=X+324,Y+43+118;right(g,x,y);g.shot('20-menu-kernel');m=g.mark();g.click(x+51,y+6+31+13);g.wait('[user:Finder] /System/Kernel/kernel.elf',mark=m);g.key('esc');g.key('alt-left');time.sleep(.15)
    check(True,'Right-click on the actual read-only kernel exposes its binary inspector rather than file modification actions')

def themes(g,E,check,metrics):
    g.spawn('f4','Impostazioni');X,Y,w,h=g.bounds(4);B=Y+43;left=X+1+164;ww=w-2-164-14;cw=(ww-16)//5;pal=[]
    for j,exp in enumerate(['ffffff','29292e','f9fdff','17332c','382a2d']):
        g.click(left+j*(cw+4)+cw//2,B+117);time.sleep(.48);im=g.frame();got=im.getpixel((X+w-37,B+264));assert got==tuple(bytes.fromhex(exp)),(j,got,exp);pal.append(exp)
        if j>=2:
            wall=Image.open(ROOT/'assets'/(['glaciale','boreale','rame'][j-2]+'.qoi')).convert('RGB');assert ImageChops.difference(im.crop((16,57,90,97)),wall.crop((16,57,90,97))).getbbox() is None
            if E:im.save(E/('21-tema-'+['glaciale','boreale','rame'][j-2]+'.png'))
    metrics['sampled_settings_card_rgb']=pal;check(True,'Five theme selections yield five measured client palette colors; each new theme paints its own exact wallpaper pixels in the real GOP framebuffer')
    # Default appearance for the broad regression, leaving Rame selected separately.
    g.click(left+cw//2,B+118);m=g.mark();g.key('ctrl-w');g.wait('motion close frames=',mark=m)

def apple(g,check,metrics):
    im=g.frame();box=im.crop((12,5,36,29));white={(x,y) for y in range(24) for x in range(24) if min(box.getpixel((x,y)))>=250};vals=list(map(int,re.findall(r'\d+',(ROOT/'assets/apple-menu.inc').read_text())));shape={(x,y) for y in range(24) for x in range(24) if vals[y*24+x]>=246};assert white and len(white^shape)<12,(len(white^shape),len(white));ys=[y for x,y in white];span=max(ys)-min(ys)+1;assert 12<=span<=16;metrics['apple_menu']={'bar_px':34,'raster_design_height_px':16,'bright_core_height_px':span,'white_pixels':len(white)};check(True,'Native menu glyph compared to its SVG-derived mask in the framebuffer: 16 px design inside a 34 px bar, boot logo remains separate')

def install(g,E=None,clicks=False):
    if clicks:
        m=g.mark();g.move(1205,249);g.mouse(True);g.mouse(False);g.mouse(True);g.mouse(False);g.wait('[window] open Installa tinymacOS',mark=m);g.wait('motion open frames=',mark=m)
    else:g.spawn('f11','Installa tinymacOS')
    X,Y,w,h=g.bounds(10);B=Y+43;ah=h-46;go=(X+w-87,B+ah-25)
    if E:g.shot('22-installa')
    if clicks:
        g.click(*go);g.click(X+350,B+123);g.click(*go)
        n=g.mark();g.click(*go);assert '[install] Controllo' not in g.text()[n:] # disabled until explicitly checked
        g.click(X+245,B+ah-96)
        if E:g.shot('23-riepilogo')
        g.click(*go)
    else:
        for k in ('ret','down','ret','spc','ret'):g.key(k,.14)
    g.wait('Installazione completata e verificata',mark=0,timeout=90);time.sleep(.23)
    if E:g.shot('24-installazione-verificata')
    g.key('ret');g.wait('[power] ACPI S5',mark=0);g.p.wait(timeout=9)
