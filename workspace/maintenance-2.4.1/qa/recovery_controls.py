#!/usr/bin/env python3
"""Test real UEFI recovery panel movement and close confirmation, no disk writes."""
import json,os,sys,time
from pathlib import Path
import numpy as np
sys.path.insert(0,'/home/user/tinymacOS/tests')
from retail_guest import Retail
from boot_recovery import RecoveryKey
class Rec(RecoveryKey,Retail):initial='Scegli come proseguire.'
w=Path('/var/tmp/tmc-rec-controls-241');w.mkdir(exist_ok=True)
e=Path(__file__).resolve().parents[1]/'evidence'
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD='/var/tmp/tmc-maintenance-241')
with Rec(Path('/var/tmp/tmc-maintenance-241/tinymacOS-2.4.1-x86_64.iso'),w,e,'recovery-controls',live=True) as g:
 g.mx,g.my=1232,745
 g.move(40,40);before=np.array(g.snap())
 g.move(600,140);g.mouse(True);g.move(620,160);g.mouse(False);g.move(40,40);time.sleep(.2)
 after=np.array(g.snap())
 assert np.array_equal(before[170:650,170:1100],after[190:670,190:1120]),'Recovery drag did not translate the content by exactly 20x20 pixels'
 g.shot('recovery-dragged')
 g.click(177,162);g.visible('Spegnere il computer?',8);g.shot('recovery-close-confirmation')
 (e/'recovery-controls.json').write_text(json.dumps({'drag_pixels':[20,20],'content_translation_pixel_exact':True,'red_close_opens_power_confirmation':True,'disk_writes_requested':False},indent=2))
 print('PASS: Recovery content translates by exactly +20,+20; red close opens real power confirmation.')
