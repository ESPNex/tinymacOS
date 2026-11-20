#!/usr/bin/env python3
from PIL import Image,ImageDraw
from pathlib import Path
D=Path(__file__).resolve().parents[1]/'assets';raw=(D/'symbols.rgba').read_bytes()[:39*16384];S=4
for kind in ('disk','notes','stickies'):
 im=Image.new('RGBA',(256,256));d=ImageDraw.Draw(im);p=lambda *n:tuple(int(x*S) for x in n)
 def rr(a,r,c):d.rounded_rectangle(p(*a),r*S,fill=c)
 if kind=='disk':
  rr((3,3,61,61),14,'#dbe3eb');rr((10,11,53,51),7,'#576679');rr((13,13,50,40),4,'#bcc9d9');rr((16,43,46,46),1,'#394452');d.ellipse(p(42,46,48,50),fill='#a2e7af');d.line([p(28,39),p(45,20)],fill='#eaf3ff',width=5*S);d.ellipse(p(38,13,53,28),fill='#5387be');d.ellipse(p(42,17,49,24),fill='#c6edff')
 elif kind=='notes':
  rr((3,3,61,61),13,'#fffbed');d.rounded_rectangle(p(3,3,61,26),radius=12*S,fill='#ffca40');d.rectangle(p(3,15,61,23),fill='#ffca40');
  for y in (32,40,48):rr((12,y,52,y+1),0,'#c7c0b2')
 else:
  rr((4,3,59,59),12,'#f3b646');rr((10,12,57,56),6,'#f9cb62');rr((6,6,54,51),5,'#fff08a');d.polygon([p(43,51),p(54,40),p(43,41)],fill='#edba48');
  for y,w in ((18,40),(25,43),(32,32)):rr((14,y,w,y+1),0,'#a18137')
 raw+=im.resize((64,64),Image.Resampling.LANCZOS).tobytes()
(D/'symbols.rgba').write_bytes(raw)

from PIL import ImageOps
ImageOps.fit(Image.open(D/'riviera-master.jpg').convert('RGB'),(1280,800),method=Image.Resampling.LANCZOS).save(D/'riviera.qoi')
