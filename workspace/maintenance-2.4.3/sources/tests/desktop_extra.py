"""Complementary disk-only UI checks: custom theme and context actions outside Finder."""
import argparse,hashlib,json,os,tempfile,time
from pathlib import Path
from PIL import Image,ImageChops
from guest import Guest,ROOT,default_iso,audit_bytes
from orizzonte import install,right,disk_bytes
args=argparse.ArgumentParser();args.add_argument('--evidence',type=Path,required=True);E=args.parse_args().evidence.resolve();E.mkdir(parents=True,exist_ok=True)
iso=Path(os.environ.get('ISO',default_iso()));results=[]
with tempfile.TemporaryDirectory(prefix='orizzonte-ui-') as tmp:
    w=Path(tmp)
    with Guest(iso,w,E,'theme-selected') as g:
        g.enroll();g.spawn('f4','Impostazioni');X,Y,W,H=g.bounds(4);g.click(X+226+20+4*40+13, Y+43+230+13);time.sleep(.30);g.click(X+226+18+45, Y+43+302+50+19);time.sleep(.40);install(g)
    disk=w/'persistent/system.raw';a=audit_bytes(disk_bytes(disk));v=a['/Users/User/Library/Preferences/com.tinymacos.appearance'];assert v[:5]==bytes((4,6,0,0,0)), f'appearance={tuple(v[:6])}';(w/'persistent/vars.fd').unlink()
    with Guest(None,w,E,'context-on-disk',boot_disk=True) as g:
        assert '[prefs] loaded theme,wall,dock,reduce,opaque: 46000' in g.text();g.unlock();im=g.frame();wall=Image.open(ROOT/'assets/rame.qoi').convert('RGB');box=(24,54,112,121);assert ImageChops.difference(im.crop(box),wall.crop(box)).getbbox() is None
        results.append('Installed SATA volume restores Rame vespertino (palette 4, wallpaper 6) after a new-firmware cold boot; raw prefs and GOP match.')
        # Shipped design: the Installer exists only on the live medium
        # (lastapp(live_cd) = 14 vs 13 apps; F11/context/dock all gate on it),
        # so from the installed volume F11 must not offer it at all.
        m=g.mark();g.key('f11');time.sleep(1.2);assert '[window] open Installa tinymacOS' not in g.text()[m:] and '[install] Controllo' not in g.text();results.append('F11 on the installed volume does not open the installer: the running disk cannot be rewritten.')
        g.spawn('f3','TextEdit');g.type('Menu reale');X,Y,W,H=g.bounds(3);x=X+186;y=Y+125
        # Copy, paste, save via the context menu. Each choice uses actual mouse clicks.
        for idx in (0,1,2):
            right(g,x,y);g.click(x+49,y+6+31*idx+14)
        g.wait('Salvato su disco',mark=m)
        # Close TextEdit before the shell reads the file back (the writer must
        # not hold the document open while `cat` opens it).
        m=g.mark();g.key('ctrl-w');g.wait('motion close frames=',mark=m)
        g.spawn('f2','Terminale');g.cmd('cat Documents/Untitled.txt','Menu realeMenu reale');results.append('TextEdit context-menu Copy, Paste and Save write the expected doubled document; Terminal reads it back from SATA.')
        right(g,880,748);m=g.mark();g.key('down');g.key('ret');g.wait('[window] open Impostazioni',mark=m);g.wait('motion open frames=',mark=m);X,Y,W,H=g.bounds(4);right(g,X+250,Y+20);m=g.mark();g.key('down');g.key('ret');g.wait('motion minimize frames=',mark=m);g.key('f4');g.wait('motion restore frames=',mark=m);g.key('f2');time.sleep(.2);g.shutdown()
        results.append('Dock icon context opens Settings; its title-bar context actually minimizes the window and F4 restores it.')
(E/'desktop-extra.json').write_text(json.dumps({'sha256':hashlib.sha256(iso.read_bytes()).hexdigest(),'checks':results},indent=2)+'\n');print('\n'.join('PASS: '+s for s in results))
