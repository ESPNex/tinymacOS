#!/usr/bin/env python3
"""Compile original SVG symbols to deterministic antialiased alpha masks.
Supported SVG subset is intentionally strict: viewBox 32, paths M/L/H/V/C/Z,
rectangles and circles, solid currentColor fill/stroke, round joins/caps.
No network, fonts, scripts or dependencies except the existing Pillow toolchain.
The SVG files are the source of truth, not C drawing routines or old sprites.
"""
from pathlib import Path
import hashlib,json,re,sys,xml.etree.ElementTree as ET
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1];NAMES='appear glass motion dock mission lock calc calendar terminal settings theme wall chevron'.split()
def render(path,size=64):
 root=ET.fromstring(path.read_bytes());assert root.tag=='{http://www.w3.org/2000/svg}svg' and root.get('viewBox')=='0 0 32 32'
 S=8;k=size*S/32;im=Image.new('L',(size*S,size*S));draw=ImageDraw.Draw(im)
 for e in root:
  tag=e.tag.split('}')[-1]
  if tag=='title':continue
  assert tag in ['path','rect','circle'],tag
  fill=e.get('fill',root.get('fill'));stroke=e.get('stroke',root.get('stroke'));assert fill in ['none','currentColor'] and stroke in ['none','currentColor']
  width=float(e.get('stroke-width',root.get('stroke-width')));assert 0<width<=4
  width=max(1,round(width*k));radius=width/2
  def xy(p):return tuple(v*k for v in p)
  if tag=='path':
   d=e.get('d');tokens=re.findall(r'[MLHVCZ]|-?(?:\d+(?:\.\d*)?|\.\d+)',d)
   assert re.sub(r'[\s,]','',''.join(tokens))==re.sub(r'[\s,]','',d),'Unsupported SVG path syntax'
   i=0;cmd=None;pos=(0.,0.);start=None;lines=[];points=[]
   def flush():
    if not points:return
    ps=[xy(p) for p in points]
    if fill!='none':draw.polygon(ps,fill=255)
    if stroke!='none' and len(ps)>1:
     draw.line(ps,fill=255,width=width,joint='curve')
     for x,y in ps:draw.ellipse((x-radius,y-radius,x+radius,y+radius),fill=255)
   while i<len(tokens):
    if tokens[i] in 'MLHVCZ':cmd=tokens[i];i+=1
    assert cmd
    if cmd=='Z':
     assert start;points.append(start);pos=start;flush();points=[];cmd=None;continue
    count={'M':2,'L':2,'H':1,'V':1,'C':6}[cmd];v=list(map(float,tokens[i:i+count]));assert len(v)==count;i+=count
    if cmd=='M':flush();points=[];pos=tuple(v);start=pos;points.append(pos);cmd='L'
    elif cmd=='L':pos=tuple(v);points.append(pos)
    elif cmd=='H':pos=(v[0],pos[1]);points.append(pos)
    elif cmd=='V':pos=(pos[0],v[0]);points.append(pos)
    elif cmd=='C':
     a=pos;b=tuple(v[:2]);c=tuple(v[2:4]);end=tuple(v[4:]);
     for step in range(1,49):
      t=step/48;u=1-t;points.append(tuple(u*u*u*a[j]+3*u*u*t*b[j]+3*u*t*t*c[j]+t*t*t*end[j] for j in [0,1]))
     pos=end
   flush()
  else:
   if tag=='rect':
    x=float(e.get('x'));y=float(e.get('y'));w=float(e.get('width'));h=float(e.get('height'));box=xy((x,y,x+w,y+h));r=float(e.get('rx','0'))*k
    draw.rounded_rectangle(box,radius=r,fill=255 if fill!='none' else None,outline=255 if stroke!='none' else None,width=width)
   else:
    x=float(e.get('cx'));y=float(e.get('cy'));r=float(e.get('r'));box=xy((x-r,y-r,x+r,y+r))
    draw.ellipse(box,fill=255 if fill!='none' else None,outline=255 if stroke!='none' else None,width=width)
 return im.resize((size,size),Image.Resampling.LANCZOS)
def build(dst):
 dst=Path(dst);dst.mkdir(parents=True,exist_ok=True);masks=[];records=[]
 for name in NAMES:
  p=ROOT/'assets/control-center'/(name+'.svg');m=render(p);assert m.getbbox();masks.append(m.tobytes());records.append({'name':name,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
 raw=b''.join(masks);(dst/'control-svg.alpha').write_bytes(raw)
 (dst/'control-svg.json').write_text(json.dumps({'source':'original SVG','rasterization':'8x supersampled alpha, 64px native mask','symbols':records,'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()},indent=2)+'\n')
 print('SVG Control Center:',len(NAMES),'original SVG sources; deterministic antialiased masks')
if __name__=='__main__':build(sys.argv[1])
