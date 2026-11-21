#!/usr/bin/env python3
"""Match the five actually drawn Recovery menu icons in the current ISO's frame."""
from pathlib import Path
import hashlib,json
import cv2,numpy as np
from PIL import Image
r=Path(__file__).resolve().parent;build=Path('/var/tmp/tmc30a4-final')
a=json.loads((r/'appearance-result.json').read_text());iso=build/'tinymacOS-10-x86_64.iso'
assert a['passed'] and hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()==a['iso_sha256']
frame=Path(a['work'])/'evidence/recovery-smaller-title.png';im=np.asarray(Image.open(frame).convert('RGB'))
icons=np.frombuffer((build/'rec.icons').read_bytes(),np.uint8).reshape(-1,64,64,4)
checks=[]
for slot,name,y in [(1,'Ripristina',226),(2,'Reinstalla',288),(3,'Utility Disco',350),(4,'Terminale',412),(5,'Disco di avvio',474)]:
 p=icons[slot][np.arange(39)*64//39][:,np.arange(39)*64//39]
 mask=np.repeat((p[:,:,3]>250)[:,:,None],3,axis=2).astype(np.uint8)*255
 score,_,xy,_=cv2.minMaxLoc(cv2.matchTemplate(im,p[:,:,:3],cv2.TM_SQDIFF_NORMED,mask=mask))
 assert score<.002 and xy==(166,y),(name,score,xy)
 checks.append({'name':name,'normalized_squared_error':score,'location':xy,'artwork':'imported' if slot==4 else 'desktop Tramontana installer procedural' if slot==2 else 'original procedural'})
record={'passed':True,'iso_sha256':a['iso_sha256'],'frame_sha256':hashlib.sha256(frame.read_bytes()).hexdigest(),'checks':checks}
(r/'recovery-visual-result.json').write_text(json.dumps(record,indent=2)+'\n')
print('PASS all five drawn Recovery menu icons: procedural except Terminal')
