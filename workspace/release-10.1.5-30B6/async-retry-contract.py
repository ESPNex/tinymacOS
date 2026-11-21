#!/usr/bin/env python3
"""Failure injection after a successful cancellation: don't cancel twice."""
from pathlib import Path
import subprocess,tempfile,json,hashlib
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS'
f=(root/'tests/recovery_usb.c').read_text();f=f[:f.index('int main(void)')]
f=f.replace('"../recovery/usbhid.c"','"'+str(root/'recovery/usbhid.c')+'"')
f=f.replace('static int state=1,','static int clear_fail,arm_fail,cancel_fail;\nstatic int state=1,')
f=f.replace('cleared++;return 0;','cleared++;if(clear_fail){clear_fail--;return 1;}return 0;')
f=f.replace('cancelled++;return 0;','cancelled++;if(cancel_fail){cancel_fail--;return 1;}return 0;')
f=f.replace('armed++;return 0;','armed++;if(arm_fail){arm_fail--;return 1;}return 0;')
f+=r'''
static void retry(void){TabletState s;for(int i=0;i<200;i++)rec_pointer_next(&bs,&s);}
int main(void){
 state=0;deny_report=1;assert(rec_pointer_start(&bs,100,100));
 clear_fail=1;received(0,0,0,2);retry();
 assert(cancelled==1&&cleared==2&&!pending_error&&submitted);
 int c=cancelled;arm_fail=1;received(0,0,0,2);retry();
 assert(cancelled==c+1&&!pending_error&&submitted);
 c=cancelled;cancel_fail=1;received(0,0,0,2);retry();
 assert(cancelled==c+2&&!pending_error&&submitted);
 TabletState s;u8 r[]={0,1,0};received(r,3,0,0);assert(rec_pointer_next(&bs,&s)&&s.x==1&&!s.y);
 puts("PASS actual Recovery async recovery: failed clear-halt or resubmit retries without double-cancelling; failed cancellation retries safely; subsequent movement preserved");
}
'''
with tempfile.TemporaryDirectory(prefix='tmc1013-async-',dir='/var/tmp') as d:
 w=Path(d);(w/'test.c').write_text(f)
 subprocess.run(['gcc','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror','-I'+str(root/'include'),str(w/'test.c'),str(root/'drivers/hidtablet.c'),'-o',str(w/'test')],check=True)
 r=subprocess.run([str(w/'test')],capture_output=True,text=True);print(r.stdout,r.stderr);assert r.returncode==0
 (q/'async-retry-result.json').write_text(json.dumps({'passed':True,'log':r.stdout,'source_sha256':hashlib.sha256((root/'recovery/usbhid.c').read_bytes()).hexdigest(),'sanitizers':['address','undefined']},indent=2)+'\n')
