#!/usr/bin/env python3
"""Finish the three original Orizzonte wallpapers and the install-assistant icon."""
from pathlib import Path
from PIL import Image,ImageOps,ImageDraw
D=Path(__file__).resolve().parents[1]/'assets'
for s in ('glaciale','boreale','rame'):
    im=ImageOps.fit(Image.open(D/(s+'-master.jpg')).convert('RGB'),(1280,800),method=Image.Resampling.LANCZOS)
    im.save(D/(s+'.qoi'))
im=Image.new('RGBA',(256,256));dr=ImageDraw.Draw(im);mask=Image.new('L',im.size);ImageDraw.Draw(mask).rounded_rectangle((8,8,247,247),radius=57,fill=255)
for y in range(256):
    t=y/255;dr.line((0,y,255,y),fill=tuple(round(a*(1-t)+b*t) for a,b in zip((249,182,133),(147,68,81)))+(255,))
dr.polygon([(0,134),(64,102),(114,126),(169,86),(214,107),(256,92),(256,228),(0,228)],fill='#96576a');dr.polygon([(0,160),(71,142),(121,157),(189,131),(256,156),(256,195),(0,195)],fill='#754b67');dr.polygon([(39,170),(216,170),(226,190),(30,190)],fill='#fff3df');dr.rounded_rectangle((30,181,226,211),9,fill='#e1d7d0');dr.rounded_rectangle((163,193,198,198),3,fill='#77737a');dr.rounded_rectangle((116,39,140,117),9,fill='#fff8e8');dr.polygon([(87,96),(169,96),(128,141)],fill='#fff8e8');im.putalpha(mask)
f=D/'symbols.rgba';assert len(f.read_bytes()) in (38*16384,39*16384);f.write_bytes(f.read_bytes()[:38*16384]+im.resize((64,64),Image.Resampling.LANCZOS).tobytes())
