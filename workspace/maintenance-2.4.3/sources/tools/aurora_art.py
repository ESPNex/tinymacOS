#!/usr/bin/env python3
"""Finishing pass for Aurora's original app marks and account portrait."""
from PIL import Image,ImageDraw,ImageFilter,ImageOps
from pathlib import Path
import numpy as np, math
D=Path(__file__).resolve().parents[1]/'assets';N=256;S=4;Y,X=np.mgrid[0:N,0:N]/S
raw=(D/'symbols.rgba').read_bytes();slots=[raw[i:i+16384] for i in range(0,len(raw),16384)][:31]+[bytes(16384) for _ in range(7)]
def rgb(s):return np.array(tuple(bytes.fromhex(s.lstrip('#'))),float)
def box(t):return tuple(round(i*S) for i in t)
def grd(a,b,curve=0):
 u=np.clip((Y+curve*(X-32))/64,0,1)[:,:,None];return Image.fromarray(np.uint8(rgb(a)*(1-u)+rgb(b)*u),'RGB').convert('RGBA')
def sq(a,b):
 g=grd(a,b,.14);im=Image.new('RGBA',(N,N));mask=Image.new('L',(N,N));ImageDraw.Draw(mask).rounded_rectangle(box((2,2,62,62)),14*S,fill=255);im.paste(g,(0,0),mask);return im
def put(i,im):slots[i]=im.resize((64,64),Image.Resampling.LANCZOS).tobytes()
def ln(d,points,c,w):d.line([tuple(round(z*S) for z in p) for p in points],fill=c,width=max(1,round(w*S)),joint='curve')
def el(d,t,c):d.ellipse(box(t),fill=c)
def rr(d,t,r,c):d.rounded_rectangle(box(t),r*S,fill=c)
def poly(d,pts,c):d.polygon([box(p) for p in pts],fill=c)
# Faceted blue face mark; original split, smile, proportions and outlines.
im=sq('7de3ff','236bdc');d=ImageDraw.Draw(im);mask=im.getchannel('A');poly(d,[(36,1),(64,1),(64,64),(30,64),(30,42),(22,41)],'#daf7ff');ln(d,[(16,20),(16,26)],'#17345b',2.6);ln(d,[(45,20),(45,26)],'#17345b',2.6);pts=[(12+i*1.6,37+6*math.sin(math.pi*i/25)) for i in range(26)];ln(d,pts,'#16335a',2.4);im.putalpha(mask);put(1,im)
# A photographic aperture made of eight translucent overlapping petals.
im=sq('ffffff','e8edf5');
for j,c in enumerate(['ffbf33','ff9426','fc5154','db469a','8666d6','498ad7','48bab5','9bcc65']):
 ov=Image.new('RGBA',(N,N));d=ImageDraw.Draw(ov);el(d,(25,7,39,33),'#'+c+'d8');ov=ov.rotate(-45*j,resample=Image.Resampling.BICUBIC,center=(N/2,N/2));im=Image.alpha_composite(im,ov)
put(19,im);im=sq('828c9d','303746');d=ImageDraw.Draw(im);rr(d,(7,8,57,55),9,'#182030');rr(d,(10,11,54,50),6,'#0e1420');ln(d,[(16,21),(23,27),(16,33)],'#f5f8ff',2.7);ln(d,[(27,34),(39,34)],'#eaf2fb',2.4);put(2,im)
im=sq('9098a7','414b5b');d=ImageDraw.Draw(im);rr(d,(10,9,54,24),4,'#283341');rr(d,(15,13,46,18),2,'#becfe0');
for j in range(3):
 for i in range(4):rr(d,(10+i*11.3,29+j*8.7,18+i*11.3,35+j*8.7),2,'#ff9f30' if i==3 else '#c6cfdf')
put(16,im)
# Use straight alpha from the user-provided vector (also used at boot).
slots[31]=Image.open(D/'apple-icon.png').convert('RGBA').tobytes()
im=sq('e9eef7','bac7d8');d=ImageDraw.Draw(im);rr(d,(8,9,55,53),9,'#3b4354');rr(d,(13,14,50,47),5,'#818e9f');rr(d,(20,22,43,40),3,'#26374f');
for j in range(4):
 x=20+j*7;ln(d,[(x,12),(x,18)],'#e1e8f0',1.3);ln(d,[(x,44),(x,50)],'#dbe4f0',1.3)
ln(d,[(25,28),(21,31),(25,34)],'#bce2ff',1.3);ln(d,[(37,28),(41,31),(37,34)],'#bce2ff',1.3);put(32,im)
im=Image.new('RGBA',(N,N));d=ImageDraw.Draw(im);rr(d,(7,15,56,50),7,'#8395b1');rr(d,(11,18,52,38),4,'#d4e0ec');ln(d,[(13,43),(49,43)],'#526883',2);el(d,(44,44,48,48),'#9ae1b4');put(33,im)
# Account picture: folded-paper fox in an apricot/rose circle.
im=grd('ffd6aa','dc8f94',.3);d=ImageDraw.Draw(im);poly(d,[(9,13),(26,22),(38,22),(55,13),(50,42),(33,55),(14,41)],'#bc5a36');poly(d,[(10,13),(22,25),(13,32)],'#f7aa68');poly(d,[(54,13),(43,25),(51,31)],'#f7aa68');poly(d,[(13,29),(24,21),(37,20),(52,29),(43,44),(33,52),(21,43)],'#f38d45');poly(d,[(13,31),(28,35),(33,50),(19,42)],'#fff1d6');poly(d,[(52,30),(37,35),(32,50),(45,42)],'#fff1d6');poly(d,[(24,22),(37,21),(33,44)],'#ffad5f');ln(d,[(19,32),(25,34)],'#583b38',1.4);ln(d,[(38,34),(44,31)],'#583b38',1.4);poly(d,[(28,42),(36,42),(32,47)],'#573e40');mask=Image.new('L',(N,N));ImageDraw.Draw(mask).ellipse(box((1,1,63,63)),fill=255);im.putalpha(mask);im.resize((128,128),Image.Resampling.LANCZOS).save(D/'account.png');(D/'account.rgba').write_bytes(im.resize((128,128),Image.Resampling.LANCZOS).tobytes());put(34,im)
for i,green in [(35,False),(36,True)]:
 im=Image.new('RGBA',(N,N));d=ImageDraw.Draw(im);d.arc(box((13,14,51,52)), -59 if green else -50,232,fill='#ffffff',width=round(4*S));
 if green:poly(d,[(47,9),(52,22),(38,21)],'white')
 else:ln(d,[(32,8),(32,30)],'white',4)
 put(i,im)
# Blue folder with system seal.
im=Image.frombytes('RGBA',(64,64),slots[6]).resize((N,N));d=ImageDraw.Draw(im);el(d,(23,29,43,49),'#458bc7');el(d,(28,34,38,44),'#92d6ff');el(d,(31,37,35,41),'#458bc7');put(37,im)
(D/'symbols.rgba').write_bytes(b''.join(slots))
