"""Black-box native framebuffer regression for Mission Control chrome and menu alignment.
No guest instrumentation: actual F10/Escape input, QMP framebuffer and shipped masks.
Optional TMC_UI_EXPECT_HALO=1 reproduces old build; never substitutes for final PASS.
"""
import hashlib,json,os,tempfile,time
from pathlib import Path
import cv2,numpy as np
from retail_guest import Retail
from guest import ROOT
iso=Path(os.environ['TMC_TEST_ISO']);w=Path(tempfile.mkdtemp(prefix='tmc-chrome-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ['TMC_POINTER']='mouse';old=os.environ.get('TMC_UI_EXPECT_HALO')=='1'
record={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'checks':[],'old_halo_reproduction':old}
def save(im,name,box):im.crop(box).save(ev/(name+'.png'),optimize=True)
def check_cycles(g,orientation):
    g.move(650,660);time.sleep(2.4)
    before=g.snap();W,H=before.size
    box=(0,34,110,H) if orientation=='left' else (0,H-110,W,H)
    src=np.asarray(before.crop(box)).astype(np.int16)
    for cycle in range(3):
        g.key('f10');time.sleep(.8);g.move(800,660);time.sleep(.25);g.move(650,660);time.sleep(.6);dim=g.snap()
        assert np.count_nonzero(np.any(np.asarray(dim.crop(box))!=src,axis=2))>200,'F10 did not change scene'
        g.key('esc' if cycle%2 else 'f10');time.sleep(1.8);after=g.snap()
        delta=np.abs(np.asarray(after.crop(box)).astype(np.int16)-src)
        count=int(np.count_nonzero(np.max(delta,axis=2)>0))
        # Count the exterior separately to distinguish the reported stale halo
        # from the material itself. Final acceptance requires ALL pixels exact.
        yy,xx=np.indices(delta.shape[:2]);yy+=box[1];xx+=box[0]
        exterior=~((xx>=10)&(xx<78)&(yy>=121)&(yy<679)) if orientation=='left' else ~((xx>=361)&(xx<919)&(yy>=H-84)&(yy<H-15))
        halo=int(np.count_nonzero(np.max(delta,axis=2)[exterior]>0))
        record['checks'].append({'orientation':orientation,'cycle':cycle,'changed_pixels_after_exit':count,'maximum_channel_delta':int(delta.max()),'exterior_halo_pixels':halo})
        save(before,orientation+'-before',box);save(after,orientation+'-after',box)
        if old:
            assert halo>200,'Old build did not reproduce the halo';return
        assert count==0 and halo==0,(orientation,cycle,halo,int(delta.max()),'chrome/backdrop pixels not restored')
    print('PASS exact Dock + shadow/backdrop restoration across 3 Mission Control cycles:',orientation,flush=True)
try:
    with Retail(iso,w,ev,'chrome',live=True,launcher=ROOT/'tools/run-input-qa.sh') as g:
        mice=g.req('query-mice');target=next(m for m in mice if m['name']=='QEMU HID Mouse')
        g.hmp('mouse_set '+str(target['index']));assert next(m for m in g.req('query-mice') if m['current'])['name']=='QEMU HID Mouse'
        record['selected_pointer_transport']='QEMU HID Mouse'
        g.enroll();check_cycles(g,'bottom')
        if not old:
            im=g.snap();a=np.asarray(im);mask=np.array([int(v) for v in (ROOT/'assets/apple-menu.inc').read_text().replace('\n','').split(',') if v.strip()],dtype=np.uint8).reshape(24,24)
            gray=cv2.cvtColor(a[:34,:45],cv2.COLOR_RGB2GRAY);_,score,_,xy=cv2.minMaxLoc(cv2.matchTemplate(gray,mask,cv2.TM_CCOEFF_NORMED));assert score>.97 and xy==(12,3),(score,xy)
            x,y=g.visible('File',faces=(1,),box=(40,0,400,34));region=a[y:y+16,x:x+25];bright=np.all(region>220,axis=2);ys=np.nonzero(bright)[0];assert len(ys)
            apple_y=np.nonzero(mask>=64)[0];center_apple=float((apple_y.min()+apple_y.max())/2+xy[1]);center_file=float((ys.min()+ys.max())/2+y)
            assert abs(center_apple-center_file)<=1,(center_apple,center_file)
            record['menu_alignment']={'glyph_origin':xy,'apple_visible_center_y':center_apple,'File_visible_center_y':center_file,'difference_px':abs(center_apple-center_file)}
            save(im,'menu-alignment',(0,0,470,34));print('PASS menu Apple glyph aligned with File to <= 1 pixel',flush=True)
            # Switch Dock through the shipping Settings UI, not a guest memory write.
            g.key('f4');g.visible('Impostazioni di Sistema');sx,sy=g.visible('Schermo');g.click(sx+16,sy+5);g.visible('RISOLUZIONE')
            px,py=g.visible('Posizione',faces=(0,));# label is client x+242; switch client x+788, same row
            g.click(px+560,py+5);g.visible('Sinistra');time.sleep(1)
            g.key('ctrl-w');time.sleep(1);check_cycles(g,'left')
        record['passed']=not old;record['reproduced']=old
finally:
    Path(os.environ['TMC_UI_RESULT']).write_text(json.dumps(record,indent=2)+'\n');print(w,flush=True)
