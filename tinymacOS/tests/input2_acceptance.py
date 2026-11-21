"""Actual native PS/2 and USB2 input acceptance, no F11 diagnostic dependency.
Runs one firmware/profile per process; all disks are disposable external files.
Env: TMC_TEST_INPUT, TMC_TEST_FIRMWARE, TMC_TEST_ISO, TMC_INPUT_RESULT.
Optional TMC_INPUT_DISK: disposable BIOS installer fixture, cloned before use.
"""
import hashlib,json,os,shutil,tempfile,time
from pathlib import Path
from guest import ROOT
from retail_guest import Retail
from input_pixels import cursor
profile=os.environ.get('TMC_TEST_INPUT','ps2');firmware=os.environ.get('TMC_TEST_FIRMWARE','uefi')
w=Path(tempfile.mkdtemp(prefix='tmc-input2-'+firmware+'-'+profile+'-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
source=os.environ.get('TMC_INPUT_DISK');iso=Path(os.environ['TMC_TEST_ISO'])
os.environ.update(TMC_POINTER='mouse',TOOLROOT='/var/tmp/tmc/tools/root')
record={'passed':False,'profile':profile,'firmware':firmware,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'physical_hardware_tested':False,'checks':[]}
class InputGuest(Retail):
    initial='BiosDisk' if source else 'Crea un account'
    def mouse(self,down):
        self.req('input-send-event',events=[{'type':'btn','data':{'button':'left','down':down}}]);time.sleep(.13)
    def button(self,key,down):
        self.req('input-send-event',events=[{'type':'btn','data':{'button':key,'down':down}}]);time.sleep(.13)
def passed(s):record['checks'].append(s);print('PASS',firmware,profile,s,flush=True)
try:
    if source:
        assert source.startswith('/var/tmp/');state=w/'disk';state.mkdir();disk=state/'system.raw';shutil.copyfile(source,disk);os.environ['DISK']=str(disk)
    with InputGuest(iso,w,ev,'input',live=not bool(source),disk='disk' if source else 'vars',boot_disk=bool(source),launcher=ROOT/'tools/run-input-qa.sh') as g:
        usb=g.hmp('info usb');record['usb']=usb;record['mice_before_selection']=g.req('query-mice');record['pci']=g.req('query-pci')
        # PS/2 initialization can reactivate QEMU's fallback mouse, especially
        # if the native USB driver never configured the device. Never let a
        # visible USB topology be mistaken for the actual input transport.
        target='QEMU PS/2 Mouse' if profile=='ps2' else 'QEMU HID Mouse'
        mice=g.req('query-mice');matches=[m for m in mice if m['name']==target];assert len(matches)==1
        g.hmp('mouse_set '+str(matches[0]['index']));record['mice']=g.req('query-mice')
        assert [m['name'] for m in record['mice'] if m['current']]==[target]
        record['selected_pointer_transport']=target
        if profile=='ps2':assert 'USB Mouse' not in usb
        else:
            assert 'QEMU USB Mouse' in usb
            if profile.startswith('ehci'):assert '480 Mb/s' in usb
            if profile.startswith('companion') or profile=='uhci':assert '12 Mb/s' in usb
            assert 'qemu-xhci' not in g.hmp('info qtree') or profile.startswith('xhci')
            if 'hub' in profile:
                assert 'QEMU USB Hub' in usb and 'usb-hub' in g.hmp('info qtree')
        if profile.endswith('-kbd'):
            assert 'QEMU USB Keyboard' in usb
            assert 'i8042' not in g.hmp('info qtree'),'USB keyboard acceptance must not fall back to PS/2'
            record['ps2_controller_present']=False
        else:assert 'USB Keyboard' not in usb
        passed('Exact topology: '+('PS/2 keyboard and mouse; no USB pointing device' if profile=='ps2' else ('USB keyboard' if profile.endswith('-kbd') else 'PS/2-only keyboard')+' and '+profile+' USB mouse'))
        blocks=g.req('query-block')
        if source:assert '.iso' not in str(blocks) and str(disk) in str(blocks)
        cursor(g,762,482);cursor(g,142,613);g.shot('login-cursor')
        passed('Rendered mouse sprite at two login/setup positions')
        assert b'Diagnostica USB' not in (Path(os.environ['TMC_BUILD'])/'desktop.elf').read_bytes()
        passed('Login executable has no F11 diagnostic panel')
        if source:
            g.click(600,626);g.type('local-test-285');g.click(746,628)
        else:
            for y,t in [(448,'NativeInput'),(489,'local-test-285'),(530,'local-test-285')]:g.click(641,y);g.type(t)
            g.click(640,575)
        g.visible('Preferiti',40);passed('Keyboard password entry plus mouse field/submit clicks open desktop')
        cursor(g,600,75);cursor(g,70,450)
        passed('Independent horizontal-only X and vertical-only Y cursor sprite positions')
        cursor(g,1003,361);cursor(g,308,115)
        x,y=g.visible('Preferiti',faces=(0,));g.move(x+200,y-40);g.mouse(True);g.move(x+287,y+1);g.mouse(False);g.move(1050,605)
        assert g.visible('Preferiti',faces=(0,))==(x+87,y+41)
        g.move(1005,502);g.button('right',True);g.button('right',False);g.visible('Apri Scrivania in Finder');g.key('esc')
        passed('Desktop sprite, exact Finder drag/release and right-click menu')
        g.key('f2');g.visible('Sessione locale');g.cmd('echo PS2_USB2_INPUT_OK','PS2_USB2_INPUT_OK')
        g.cmd('sw_vers','ProductVersion:    10.1.2');g.visible('BuildVersion:      30B3')
        passed('Native keyboard F2, uppercase/underscore command typing and version 30B3')
        # PS/2 negotiates a wheel packet; EHCI/UHCI prefer validated Report protocol.
        # xHCI now prefers the validated Report layout, including wheel.
        if True:
            x,y=g.visible('su console',faces=(5,));g.move(x+18,y+47)
            g.button('wheel-up',True);g.button('wheel-up',False);g.visible('Cronologia',8,faces=(0,))
            g.button('wheel-down',True);g.button('wheel-down',False);g.visible('Sessione locale',8,faces=(0,))
            passed('Mouse wheel up opens terminal history; down closes it')
        if source:
            g.cmd('cat Documents/bios-proof.txt','BIOS_DISK_PERSISTENT');passed('Installed disk document preserved, ISO absent')
            value='àèéìòù€@#'
            g.cmd('echo -n "'+value+'" > Documents/italiano-installato.txt')
            g.cmd('shasum -a 256 Documents/italiano-installato.txt',hashlib.sha256(value.encode()).hexdigest())
            passed('Installed Italian accents, euro and AltGr typed into a UTF-8 file, exact SHA-256 verified')
        g.shot('desktop-terminal');record['final_mice']=g.req('query-mice')
        assert [m['name'] for m in record['final_mice'] if m['current']]==[target]
        record['passed']=True
finally:
    target=Path(os.environ.get('TMC_INPUT_RESULT',str(w/'result.json')));target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(record,indent=2)+'\n');print(w,flush=True)
