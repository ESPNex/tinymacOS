"""Read the shipping bitmap typeface; compare words against screen pixels, not guest memory."""
from functools import lru_cache
from pathlib import Path
import struct,cv2,numpy as np
cv2.setNumThreads(1)
b=(Path(__file__).resolve().parents[1]/'assets/type.tfn').read_bytes();nf,ng=struct.unpack_from('<II',b,4);codes={c:j for j,c in enumerate(struct.unpack_from(f'<{ng}I',b,12))};start=12+ng*4;pix=start+nf*(ng*8+4)
@lru_cache(maxsize=256)
def word(text,f):
 p=start+f*(4+ng*8);h,=struct.unpack_from('<I',b,p);gs=[struct.unpack_from('<HHI',b,p+4+8*codes.get(ord(c),31)) for c in text];s=np.zeros((h,sum(x[0] for x in gs)+4),np.uint8);at=0
 for adv,w,o in gs:
  im=np.frombuffer(b,np.uint8,count=h*w,offset=pix+o).reshape(h,w);s[:,at:at+w]=np.maximum(im,s[:,at:at+w]);at+=adv
 v,u=np.nonzero(s);return s[max(0,v.min()-1):v.max()+2,max(0,u.min()-1):u.max()+2].copy()
def locate(im,text,faces=(0,1,2,3,5,6,8),threshold=.88):
 best=(0,None);gray=cv2.cvtColor(np.asarray(im),cv2.COLOR_RGB2GRAY)
 for f in faces:
  p=word(text,f)
  if p.shape[0]>gray.shape[0] or p.shape[1]>gray.shape[1]:continue
  r=cv2.matchTemplate(gray,p,cv2.TM_CCOEFF_NORMED);mn,mx,a,b=cv2.minMaxLoc(r);hit=(mx,b) if mx>-mn else (-mn,a)
  if hit[0]>best[0]:best=hit
  if hit[0]>=.99:break
 return best if best[0]>=threshold else None
