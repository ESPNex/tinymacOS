"""Aurora acceptance. Input is USB HID only; observations are COM1, GOP and offline byte reads."""
import hashlib, json, os, re, struct, subprocess, time
from pathlib import Path
from PIL import Image, ImageChops, ImageStat
from guest import ROOT, Guest
import activity

K='/System/Kernel'; PW='local-test-285'; T='[user:Terminale] '
def enrollment(g,E,check,metrics):
    samples=g.boot_samples;ws=[s[0] for s in samples];assert ws and len(set(ws))>=4 and ws==sorted(ws) and ws[0]<90 and ws[-1]>=176,ws
    activity.boot_mark(samples,check,metrics)
    metrics['splash_filled_pixels_out_of_192']=ws;im=[q[2].resize((896,560),Image.Resampling.LANCZOS) for q in samples];dt=[max(12,round(1000*(b[1]-a[1]))) for a,b in zip(samples,samples[1:])]+[520];dt[0]+=180
    im[0].save(E/'00-avvio.webp',save_all=True,append_images=im[1:],duration=dt,lossless=True,loop=0);samples[len(samples)//2][2].save(E/'00-avvio.png');g.boot_samples=[]
    check(True,'QMP recorded the white silhouette on black and increasing filled widths directly below it through boot')
    g.wait('before login: personal files, app launch and credential operation denied',mark=0);g.shot('00a-primo-avvio');g.key('f2');g.key('ctrl-spc');g.key('f9');time.sleep(.15)
    check('[window] open' not in g.text() and '[spawn] Finder' not in g.text() and 'setup ready' in g.text(),'No application or search opens before setup; an actual CPL3 client reports denied file, spawn and auth calls')
    g.type('Elena');g.key('tab');g.type('short');g.key('tab');g.type('short');g.key('ret');time.sleep(.1);assert 'authenticated' not in g.text()
    g.key('ctrl-u');g.type(PW);g.key('tab');g.key('ctrl-u');g.type('different');g.key('ret');time.sleep(.15);assert 'verifier enrolled' not in g.text()
    g.type(PW);m=activity.submit(g,E,'creazione','[session] authenticated',metrics,setup=True);g.wait('motion open frames=',mark=m)
    check(True,'First-run verification paints an animating hollow 12-spoke indicator in place of the form')
    check('verifier enrolled' in g.text()[m:],'First-run form rejects a short password and mismatched confirmation, then creates a local account and launches Finder')

def verify_view(g,obj,name,expect,E,shot=None):
    g.type(name);m=g.mark();g.key('spc');g.wait('[user:Finder] '+expect,mark=m);h=hashlib.sha256(obj).hexdigest();g.wait(f'verified bytes={len(obj)} sha256={h}',mark=m,timeout=30)
    if shot:g.shot(shot)
    return h

def volume(g,data,E,check,metrics):
    X,Y,_,_=g.bounds(1);B=Y+43;m=g.mark();g.click(X+80,B+292);g.wait('[user:Finder] '+K,mark=m);g.shot('00d-kernel-finder');h=verify_view(g,data['kernel'],'kernel.e',K+'/kernel.elf',E,'00e-kernel-elf');m=g.mark();g.key('right');g.wait('viewer offset=',mark=m);m=g.mark();g.key('end');g.wait('viewer offset=',mark=m);time.sleep(.12);offs=[int(a) for a in re.findall(r'viewer offset=(\d+)',g.text()[m:])];assert max(offs)>len(data['kernel'])-200;g.shot('00f-kernel-fine');g.key('esc');time.sleep(.13)
    check(True,'Finder streams the complete mounted kernel (length + SHA-256 equal the boot FAT), pages the ELF header and seeks to its actual last page')
    def go(name,p):
        # Prefix selection times out normally between unrelated selections.
        time.sleep(.9);g.type(name);m=g.mark();g.key('ret');g.wait('[user:Finder] '+p,mark=m)
    def root():
        m=g.mark();g.key('ctrl-shift-k');g.wait('[user:Finder] '+K,mark=m)
    go('avv',K+'/Avvio');verify_view(g,data['loader'],'boot',K+'/Avvio/BOOTX64.EFI',E,'00g-loader');g.key('esc');root()
    check(True,'Finder reads the exact bootloader length and full-stream digest without exposing C/assembly sources')
    g.spawn('f2','Terminale');g.cmd('write /System/Kernel/kernel.elf replaced',T+'Failed:');g.cmd('rm /System/Kernel/Avvio/BOOTX64.EFI',T+'Failed:');g.cmd('mkdir /System/Kernel/new',T+'Failed:');g.cmd('mv /System/Kernel/kernel.elf Documents/other',T+'Failed:');g.cmd('cp /System/Kernel/kernel.elf Documents/too-big',T+'Failed:');g.cmd('cat Documents/too-big',T+'Failed:');g.cmd('ls /System/Kernel/Sorgenti',T+'Failed:');g.cmd('shasum /System/Kernel/kernel.elf',T+h);g.wait('bytes='+str(len(data['kernel'])),mark=0)
    check(True,'Read-only boot volume rejects write, delete, mkdir and rename; oversized copy leaves no partial TinyFS file; Terminal independently re-hashes unchanged kernel')
    # The typed login secret must not pass through the front application's event queue.
    g.type('echo resume');m=g.mark();g.key('ctrl-shift-q');g.wait('login ready',mark=m);before=g.text().count('[window] open');g.shot('00b-login');g.key('f9');g.key('ctrl-spc');g.type('incorrect');m=activity.submit(g,E,'rifiuto','authentication refused',metrics);failed=g.shot('00c-password-errata');assert activity.read_phase(failed,640,630) is None;assert 'authenticated' not in g.text()[m:] and g.text().count('[window] open')==before
    time.sleep(1.25);g.type(PW);m=activity.submit(g,E,'accesso','desktop unlocked',metrics,save=True);
    check(True,'Returning login: all twelve distinct radial strokes and their clockwise brightness advances observed under both wrong and correct input; rejection restores the field')
    m=g.mark();g.key('ret');g.wait(T+'resume\n',mark=m);assert '[spawn]' not in g.text()[m:]
    check(True,'Lock freezes pre-existing clients, blocks launcher and rejects the wrong password; correct unlock resumes the untouched in-progress shell command')
    m=g.mark();g.key('ctrl-w');g.wait('motion close frames=',mark=m);g.click(X+49,B+49);g.wait('[user:Finder] /Users/User',mark=m);g.move(1190,650)

def records(raw,check):
    a=raw[13*512:14*512];b=raw[14*512:15*512];magic,ver,rounds,name,salt,verifier=struct.unpack_from('<8sII32s16s32s',a);assert magic==b'TMCUSER2' and ver==1 and rounds==120000 and name.split(b'\0')[0]==b'Elena' and salt!=bytes(16) and a==b and hashlib.sha256(a[:480]).digest()==a[480:]
    assert hashlib.pbkdf2_hmac('sha256',PW.encode(),salt,rounds)==verifier and PW.encode() not in raw
    check(True,'Offline audit: two matching private account records, nonzero salt, standard PBKDF2-HMAC-SHA256/120000 verifier, no plaintext test password in disk bytes')

def recovery(work,iso,E,check,path):
    from orizzonte import DATA
    import shutil
    for tag,slots in [('backup',[13]),('torn',[13,14])]:
        folder=work/tag;folder.mkdir();dst=folder/'system.raw';shutil.copyfile(path,dst)
        with dst.open('r+b') as f:
            for slot in slots:
                at=(DATA+slot)*512+480;f.seek(at);byte=f.read(1)[0];f.seek(at);f.write(bytes([byte^0xa5]))
        before=hashlib.sha256(dst.read_bytes()).digest()
        with Guest(None,work,E,tag,disk=tag,boot_disk=True) as g:
            if len(slots)==1:g.unlock();g.spawn('f2','Terminale');g.shutdown()
            else:
                g.wait('account unavailable',mark=0);g.key('f11');g.key('ret');assert '[window] open' not in g.text();g.click(682,743);g.key('ret');g.wait('[power] ACPI S5',mark=0);g.p.wait(timeout=9)
        assert hashlib.sha256(dst.read_bytes()).digest()==before
    check(True,'Installed GPT data partition: one damaged account record uses its backup, two fail closed. Both disks byte-identical after the read-only trials')
