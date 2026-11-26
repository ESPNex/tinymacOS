#!/usr/bin/env python3
"""Package four original artworks, draw Aurora's symbol family, and rasterize Latin UI fonts.
Only needed when changing assets. Normal builds use checked-in files and do not need PIL or NumPy.
"""
from pathlib import Path
import struct, math
import numpy as np
from PIL import Image,ImageDraw,ImageFont,ImageOps
D=Path(__file__).resolve().parents[1]/'assets'
for s in ('prisma','alba','laguna','aurora'):
 im=ImageOps.fit(Image.open(D/(s+'-master.jpg')).convert('RGB'),(1280,800),method=Image.Resampling.LANCZOS);im.save(D/(s+'.qoi'))
# Fonts are built ONLY by interface-font.py from the checked-in supplied family.
# Artwork regeneration must never overwrite assets/type.tfn.
S=4;N=64*S;Y,X=np.mgrid[0:N,0:N]/S
rgb=lambda s:np.array(list(bytes.fromhex(s)),float)
allpix=[]
for k in range(31):
 im=Image.new('RGBA',(N,N));d=ImageDraw.Draw(im)
 scale=lambda p:tuple(round(i*S) for i in p)
 def rr(p,r,c):d.rounded_rectangle(scale(p),r*S,fill=c)
 def el(p,c):d.ellipse(scale(p),fill=c)
 def line(ps,c,n=2):d.line([scale(q) for q in ps],fill=c,width=round(n*S),joint='curve')
 def poly(ps,c):d.polygon([scale(q) for q in ps],fill=c)
 def arc(p,start,end,c,n=2):d.arc(scale(p),start,end,fill=c,width=round(n*S))
 def badge(a,b,light):
  mask=Image.new('L',(N,N));ImageDraw.Draw(mask).rounded_rectangle(scale((2,2,62,62)),14*S,fill=255);g=np.clip((Y+.18*X)/72,0,1)[...,None];A=rgb(a)*(1-g)+rgb(b)*g;h=np.exp(-((X-20)**2/440+(Y-4)**2/200))*.48;A=A*(1-h[...,None])+rgb(light)*h[...,None];grad=Image.fromarray(np.uint8(A),'RGB').convert('RGBA');im.paste(grad,(0,0),mask);d.rounded_rectangle(scale((2,2,62,62)),14*S,outline=tuple((rgb(light)*.78+rgb(b)*.22).astype(int))+(255,),width=2);line([(14,4),(47,4)],'#ffffff',.55)
 if k==1:
  badge('6dccff','2758e3','e2faff');rr((11,18,31,32),4,'#2572d6');rr((11,22,53,47),5,'#1855ab');rr((16,20,47,35),3,'#eeffff');poly([(10,29),(54,26),(49,47),(16,49)],'#a6eaff');line([(13,30),(51,27)],'#f2ffff',1.6);line([(21,37),(33,36)],'#4b96d5',2)
 elif k==2:
  badge('50637d','111827','a4bfcd');rr((9,13,55,52),6,'#102031');line([(11,23),(53,23)],'#51627b',1);el((15,17,18,20),'#f997a2');el((21,17,24,20),'#f7c47f');el((27,17,30,20),'#87dcb4');line([(17,29),(24,35),(17,41)],'#dff9ff',2.6);line([(29,41),(40,41)],'#9eecdf',2.5)
 elif k==3:
  badge('fbfbff','bdc9e7','ffffff');rr((15,8,45,54),3,'#f9fbff');line([(21,22),(39,22)],'#a6b4cd',1.7);line([(21,28),(37,28)],'#a6b4cd',1.7);line([(21,34),(34,34)],'#a6b4cd',1.7);poly([(29,42),(47,15),(51,18),(33,45),(26,48)],'#3e8ef2');poly([(26,48),(29,42),(33,45)],'#e1af75');line([(32,41),(47,19)],'#b5e6ff',1.2)
 elif k==4:
  badge('aeb9ca','566375','eef6ff');el((12,12,52,52),'#576473');
  for i in range(12):
   a=math.tau*i/12;line([(32+15*math.cos(a),32+15*math.sin(a)),(32+21*math.cos(a),32+21*math.sin(a))],'#e1e8ee',3.8)
  el((17,17,47,47),'#e1e8ee');el((21,21,43,43),'#77879b');el((25,25,39,39),'#35475c');arc((25,25,39,39),190,340,'#b9cadf',1.3)
 elif k in (5,14):
  if k==5:badge('beacf8','5657bf','fff7ff')
  l=11 if k==5 else 5;t=10 if k==5 else 5;sf=.79 if k==5 else .98
  pts=lambda arr:[(l+x*sf,t+y*sf) for x,y in arr]
  poly(pts([(1,31),(20,3),(33,3),(15,31)]),'#e8fcff');poly(pts([(1,31),(15,31),(27,47),(15,47)]),'#6cc4ec');poly(pts([(17,31),(36,3),(47,19),(29,46)]),'#adbaff');line(pts([(18,31),(35,6)]),'#fffaff',1.5)
 elif k==6 or 23<=k<=26:
  rr((4,13,30,30),4,'#438bd6');rr((4,18,60,53),5,'#4ca0e6');rr((7,20,57,40),4,'#ccefff');rr((4,25,60,55),5,'#75c6fa');line([(8,26),(55,26)],'#bceaff',1.5);line([(8,53),(55,53)],'#56a7e6',1)
  if k==23:rr((23,32,42,43),2,'#489dd4');line([(28,47),(37,47)],'#489dd4',1.8)
  if k==24:
   for j,L in enumerate([16,13,16]):line([(24,34+4*j),(24+L,34+4*j)],'#489dd4',1.7)
  if k==25:line([(32,31),(32,44),(27,39)],'#489dd4',2);line([(32,44),(37,39)],'#489dd4',2)
  if k==26:poly([(21,44),(28,35),(33,40),(37,36),(43,44)],'#489dd4');el((35,31,39,35),'#489dd4')
 elif k==7:
  poly([(15,7),(39,7),(51,20),(51,57),(15,57)],'#a0b1cc');poly([(13,5),(38,5),(50,18),(50,55),(13,55)],'#f8fbff');poly([(38,5),(50,18),(38,18)],'#c6d4e9');
  for i in range(4):line([(20,24+6*i),(41-i%2*5,24+6*i)],'#93a5c4',1.5)
 elif k in (8,9,10,11,12,13,21,22,27,28,29,30):
  c='#4276d6'
  if k==8:line([(8,32),(32,10),(56,32)],c,4);line([(16,30),(16,52),(47,52),(47,30)],c,4);line([(28,52),(28,39),(36,39),(36,52)],c,3)
  if k==9:line([(8,12),(55,12),(55,44),(8,44),(8,12)],c,3.7);line([(32,45),(32,51)],c,3);line([(21,52),(44,52)],c,3)
  if k==10:line([(32,9),(32,40),(20,28)],c,4);line([(32,40),(44,28)],c,4);line([(11,42),(11,51),(53,51),(53,42)],c,4)
  if k==11:line([(8,11),(55,11),(55,52),(8,52),(8,11)],c,3.7);line([(10,46),(24,29),(34,40),(42,31),(54,46)],c,3.2);el((37,18,44,25),c)
  if k==12:rr((6,15,58,49),6,'#7485a2');rr((10,34,54,45),3,'#c4d9f0');el((44,36,49,41),'#397bdf')
  if k==13:
   for j in range(4):i=j%2;t=j//2;rr((9+i*25,9+t*25,28+i*25,28+t*25),4,c)
  if k==21:arc((9,7,43,42),0,360,'#eaf4ff',4);line([(39,38),(55,54)],'#eaf4ff',4)
  if k==22:
   for j,cx in [(21,24),(42,41)]:rr((6,j-5,57,j+5),5,'#eaf4ff');el((cx-6,j-6,cx+6,j+6),'#a8bfdc');el((cx-3,j-3,cx+3,j+3),'#344567')
  if k==27:arc((11,10,52,51),0,360,c,3.7);line([(32,18),(32,32),(41,38)],c,3.5)
  if k==28:line([(11,22),(32,9),(53,22),(32,36),(11,22)],c,4);line([(11,33),(32,47),(53,33)],c,4)
  if k==29:line([(10,35),(23,35),(28,20),(35,46),(42,30),(54,30)],c,4)
  if k==30:line([(26,12),(26,51)],c,5);line([(37,12),(37,51)],c,5)
 elif k==15:
  poly([(7,3),(51,36),(33,37),(43,54),(34,59),(24,42),(12,52)],'#17223b');poly([(12,10),(43,33),(27,34),(37,52),(34,54),(24,36),(16,45)],'#fcfdff')
 elif k==16:
  badge('ffba63','eb7735','fff4bb');rr((14,8,50,56),6,'#452d37');rr((19,14,45,24),2,'#f7dbc6');
  for j in range(3):
   for i in range(3):rr((19+i*10,29+j*8,25+i*10,34+j*8),2,'#ffac61' if i==2 else '#d4c8c9')
 elif k==17:
  badge('ffb2c0','ee5e70','ffe4e4');rr((10,12,54,53),6,'#fff8f8');rr((10,12,54,26),5,'#cf4966');line([(12,23),(52,23)],'#cf4966',4);rr((20,9,23,17),2,'#f9cdcf');rr((41,9,44,17),2,'#f9cdcf');
  for j in range(2):
   for i in range(3):rr((18+i*10,31+j*9,23+i*10,36+j*9),1,'#e3657b' if j==1 and i==0 else '#b1b4c7')
 elif k==18:
  badge('437b78','235052','b3f4ce');rr((9,12,55,52),5,'#173b43');
  for i in range(3):line([(13,22+8*i),(51,22+8*i)],'#2e565d',.9)
  line([(12,35),(21,35),(25,28),(29,40),(35,20),(40,35),(50,35)],'#9bf7bd',2.3)
 elif k==19:
  badge('8ea2ff','6554c9','e3daff');poly([(12,46),(28,25),(37,38),(44,29),(54,46)],'#c9c2fe');el((38,13,47,22),'#ffe8ad');line([(12,49),(52,49)],'#e6e6fe',1.5)
 elif k==20:
  badge('7282a9','434b77','c7d0ea');
  for j in range(3):
   for i in range(3):rr((13+14*i,13+14*j,23+14*i,23+14*j),3,['#b3ecff','#cad2ff','#e1bffa','#daf5f1','#fbddad','#ffd1d8','#b5e9d8','#dfd2ff','#c6deff'][3*j+i])
 allpix.append(im.resize((64,64),Image.Resampling.LANCZOS).tobytes())
(D/'symbols.rgba').write_bytes(b''.join(allpix))

import runpy
runpy.run_path(str(D.parent/"tools/bootart.py"))
runpy.run_path(str(D.parent/"tools/aurora_art.py"))

runpy.run_path(str(D.parent/"tools/orizzonte_art.py"))

runpy.run_path(str(D.parent/"tools/riviera_art.py"))
