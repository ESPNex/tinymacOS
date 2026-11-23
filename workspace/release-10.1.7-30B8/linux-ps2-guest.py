#!/usr/bin/env python3
"""Original Linux libps2 command/RX on actual ISO with PS/2 keyboard+mouse.
Native i8042, keyboard decoder/framing and timer-safe LEDs are NOT claimed
as Linux drivers; no USB keyboard/mouse is attached in these virtual profiles.
"""
from pathlib import Path
import hashlib,json,os,struct,subprocess,sys,tempfile,time
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS';sys.path.insert(0,str(root/'tests'))
from retail_guest import Retail
from input_pixels import cursor
fw=sys.argv[1];assert fw in ['uefi','bios']
base=Path(os.environ.get('TMC_LINUX_STAGE','/var/tmp/tmc-10.1.7/linux-hid-v12'));build=base/'build';iso=base/'tinymacOS-10.1.7-x86_64.iso'
w=Path(tempfile.mkdtemp(prefix=f'tmc-linux-ps2-{fw}-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',OCR_ROOT='/var/tmp/tmc/tools/root',TMC_MEM='512',TMC_BUILD=str(build),TMC_TEST_INPUT='ps2',TMC_TEST_FIRMWARE=fw,TMC_POINTER='mouse')
r={'passed':False,'firmware':fw,'profile':'ps2-keyboard-and-mouse','iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'actual_upstream_libps2':True,'full_linux_i8042_driver_port':False,'linux_atkbd_driver_port':False,'linux_psmouse_driver_port':False,'physical_hardware_tested':False,'checks':[]}
try:
 symbols=subprocess.check_output(['nm',str(build/'kernel.debug')],text=True).splitlines()
 def address(name):return int(next(s.split()[0] for s in symbols if s.endswith(' '+name)),16)
 for name in ['ps2_command','__ps2_command','ps2_interrupt','ps2_sendbyte','ps2_sliced_command','tmc_linux_ps2_init']:address(name)
 tool='/var/tmp/tmc/tools/root';env={**os.environ,'PATH':tool+'/usr/bin:'+os.environ['PATH'],'LD_LIBRARY_PATH':tool+'/usr/lib/x86_64-linux-gnu'}
 subprocess.run(['xorriso','-osirrox','on','-indev',str(iso),'-extract','/System/Library/KernelCollections/KernelCache',str(w/'actual.cache')],env=env,check=True,capture_output=True)
 assert (w/'actual.cache').read_bytes()==(build/'KernelCache').read_bytes();r['actual_iso_kernel_equals_build']=True
 with Retail(iso,w,ev,'linux-ps2',live=True,launcher=root/'tools/run-input-qa.sh') as g:
  r['usb']=g.hmp('info usb');assert 'USB Keyboard' not in r['usb'] and 'USB Mouse' not in r['usb']
  mice=g.req('query-mice');target=next(m for m in mice if m['name']=='QEMU PS/2 Mouse');g.hmp('mouse_set '+str(target['index']))
  def read(name,size):
   p=w/'counters.bin';g.req('pmemsave',val=address(name),size=size,filename=str(p));return p.read_bytes()
  def counters():return dict(zip(['setups','commands','command_errors','tx_bytes','rx_bytes','data_bytes','rx_errors','wait_polls','rejected'],struct.unpack('<9Q',read('tmc_linux_ps2_counters',72))))
  def integer(name):return struct.unpack('<i',read(name,4))[0]
  r['before']=before=counters();assert before['setups']==1 and before['commands']>=9 and not before['command_errors'] and before['wait_polls']>0,before
  assert integer('keyboard_on')==integer('mouse_on')==1
  def mouse_counters():return dict(zip(['starts','stops','bytes','packets','sync_drops','overflow_drops','rx_errors','protocol','packet_bytes'],struct.unpack('<9Q',read('tmc_linux_psmouse_counters',72))))
  r['mouse_before']=mouse_counters();r['negotiated_mouse_packet_bytes']=r['mouse_before']['packet_bytes'];assert r['negotiated_mouse_packet_bytes']==4
  assert r['mouse_before']['protocol'] in [5,6]
  for name in ['psmouse_process_byte','psmouse_report_standard_motion','psmouse_report_standard_buttons']:address(name)

  r['checks'].append('Original Linux libps2 executes real native keyboard setup and IntelliMouse wheel negotiation; both PS/2 ports enabled without USB HID')
  g.enroll('PsDueLinux');g.key('f2');g.visible('Sessione locale');g.cmd('echo ITA-àèéìòù-£-€-@-#','ITA-àèéìòù-£-€-@-#')
  g.key('caps_lock');g.type('echo caps');g.key('caps_lock');g.key('ret');g.visible('ECHO');time.sleep(.5)
  assert integer('led_phase')==0 and integer('led_applied')==2
  r['checks'].append('Actual PS/2 keyboard enrollment/F2/Italian Shift/AltGr/accent/euro/Caps, with nonblocking native LED ACK through Linux RX')
  for x,y in [(762,482),(142,482),(142,613),(762,613),(142,613),(762,482)]:cursor(g,x,y)
  r['exact_rendered_xy_checks']=6
  g.hmp('mouse_move 0 0 1');g.visible('Cronologia');g.hmp('mouse_move 0 0 -1');g.visible('Sessione locale')
  r['checks'].append('Actual PS/2 mouse: six exact rendered X/Y cursor checks and bidirectional wheel switches Terminal scrollback status')
  r['after']=after=counters();assert after['commands']==before['commands'] and after['rx_bytes']>before['rx_bytes']+100 and after['data_bytes']>before['data_bytes']+100 and not after['rx_errors'] and not after['rejected'],(before,after)
  r['mouse_after']=mouse_counters();assert r['mouse_after']['packets']>r['mouse_before']['packets']+20 and not r['mouse_after']['rx_errors']
  r['actual_upstream_psmouse_packet_and_detect_functions']=True
  g.shot('ps2-italian-mouse-wheel');r['passed']=True
 print('PASS Linux libps2 + actual native PS/2 keyboard/mouse input',fw,r['after'],flush=True)
finally:(q/f'linux-ps2-{fw}.json').write_text(json.dumps(r,indent=2,ensure_ascii=False)+'\n')
