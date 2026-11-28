"""Retail black-box pointer/font/window checks. Pixels and ordinary USB input only.
No debug geometry, guest memory inspection, fixture document injection or fake UI.
"""
from pathlib import Path
import struct,time
import numpy as np
from pixels import locate,b,nf,ng,start,pix,codes
ROOT=Path(__file__).resolve().parents[1]
def geometry(s,f):
 p=start+f*(4+ng*8);h,=struct.unpack_from('<I',b,p);gs=[struct.unpack_from('<HHI',b,p+4+8*codes.get(ord(c),31)) for c in s]
 image=np.zeros((h,sum(t[0] for t in gs)+4),np.uint8);at=0
 for adv,w,o in gs:
  g=np.frombuffer(b,np.uint8,h*w,pix+o).reshape(h,w);image[:,at:at+w]=np.maximum(image[:,at:at+w],g);at+=adv
 yy,xx=np.nonzero(image);return max(0,int(xx.min())-1),max(0,int(yy.min())-1),at
def launch(g,name,label):g.key('meta_l-spc');g.type(name);g.key('ret');g.visible(label);time.sleep(.5)
def cursor(g,x,y,name,label):
 raw=(ROOT/'assets/cursors'/name).read_bytes();w,h,_,_,hx,hy,n,off=struct.unpack_from('<BBBBHHII',raw,6);assert (w,h)==(32,32)
 rgba=np.frombuffer(raw,np.uint8,32*32*4,off+40).reshape(32,32,4)[::-1].copy();rgb=rgba[:,:,[2,1,0]].astype(np.int32);a=rgba[:,:,3].astype(np.int32)
 g.move(45,745);before=np.asarray(g.snap()).astype(np.int32);g.move(x,y);after=np.asarray(g.snap()).astype(np.int32)
 l,t=x-hx,y-hy;x0,y0=max(0,l),max(0,t);x1,y1=min(1280,l+32),min(800,t+32)
 a=a[y0-t:y1-t,x0-l:x1-l];rgb=rgb[y0-t:y1-t,x0-l:x1-l];back=before[y0:y1,x0:x1];actual=after[y0:y1,x0:x1]
 expected=(back*(256-a[:,:,None])+rgb*a[:,:,None])>>8;expected=np.where((a==255)[:,:,None],rgb,expected)
 mask=a>32;errors=np.max(np.abs(actual-expected),axis=2);agreement=float(np.mean(errors[mask]<=3));assert agreement>.97,(label,name,agreement,int(errors[mask].max()))
 g.shot(label);return {'asset':name,'hotspot':[hx,hy],'pointer':[x,y],'matched_visible_pixels':int(mask.sum()),'agreement':agreement}
def textedit_box(g):
 label='WiWi';shot=g.snap();hit=locate(shot,label,(1,));assert hit,'Missing proportional body glyphs';x,y=hit[1];gx,gy,_=geometry(label,1);bx=x-gx-29;by=y-gy-43-70
 # The client document card is opaque. Read its real right/bottom edges,
 # rather than correlating a tiny footer label over textured Liquid Glass.
 image=np.asarray(shot);sy=by+180;sx=bx+50;target=image[sy,sx]
 right=bx+200
 while right<1280 and np.array_equal(image[sy,right],target):right+=1
 bottom=by+180
 while bottom<800 and np.array_equal(image[bottom,sx],target):bottom+=1
 assert right<1280 and bottom<800 and right>bx+400 and bottom>by+280,(bx,by,right,bottom)
 return bx,by,right-bx+1,bottom-by+37

def edgepoint(q,e):
 x,y,w,h=q;return (x+2 if 'l' in e else x+w-3 if 'r' in e else x+w//2,y+2 if 't' in e else y+h-3 if 'b' in e else y+h//2)
def motion(g,x,y,dx,dy,frames=False):
 g.move(x,y);g.mouse(True);samples=[];begin=time.monotonic()
 for j in range(1,13):
  xx,yy=round(x+dx*j/12),round(y+dy*j/12)
  g.req('input-send-event',events=[{'type':'abs','data':{'axis':a,'value':round(v*32767/(n-1))}} for a,v,n in [('x',xx,1280),('y',yy,800)]])
  g.mx,g.my=xx,yy;time.sleep(.035)
  if frames and j%3==0:
   hit=locate(g.snap(),'WiWi',(1,));assert hit;samples.append(hit[1])
 g.mouse(False);time.sleep(.5)
 return {'elapsed_host_seconds':time.monotonic()-begin,'intermediate_text_positions':samples}
def check(g,environment,ok):
 results={'environment':environment,'cursors':[],'resizes':[]}
 launch(g,'Terminale','Sessione locale');g.cmd('echo WiWi WWW iii caffè € > Documents/m7-body.txt');g.cmd('open Documents/m7-body.txt');g.visible('WiWi',faces=(1,));q=textedit_box(g);x,y,w,h=q
 assert 750<=w<=762 and 500<=h<=530,q
 results['cursors'].append(cursor(g,x+100,y+160,'Text.cur',environment+'-text-cursor'))
 results['cursors'].append(cursor(g,x+400,y+25,'Normal.cur',environment+'-normal-cursor'))
 # Exact static art/hotspots on two edge axes and both diagonal orientations.
 for edge,asset in [('l','Horizontal Resize.cur'),('b','Vertical Resize.cur'),('lt','Diagonal Resize 1.cur'),('rt','Diagonal Resize 2.cur')]:
  px,py=edgepoint(q,edge);results['cursors'].append(cursor(g,px,py,asset,environment+'-cursor-'+edge))
 results['move']=motion(g,x+250,y+22,-70,-25,True);moved=textedit_box(g)
 assert abs(moved[0]-(x-70))<=1 and abs(moved[1]-(y-25))<=1,(q,moved)
 assert len(set(tuple(p) for p in results['move']['intermediate_text_positions']))>=3
 g.shot(environment+'-moved-window')
 for edge,dx,dy in [('l',-18,0),('r',18,0),('t',0,-12),('b',0,12),('lt',-12,-12),('rt',12,-12),('lb',-12,12),('rb',12,12)]:
  old=textedit_box(g);px,py=edgepoint(old,edge);motion(g,px,py,dx,dy);new=textedit_box(g)
  expected=(old[0]+(dx if 'l' in edge else 0),old[1]+(dy if 't' in edge else 0),old[2]+(-dx if 'l' in edge else dx if 'r' in edge else 0),old[3]+(-dy if 't' in edge else dy if 'b' in edge else 0))
  until=time.monotonic()+15
  while not all(abs(a-b)<=1 for a,b in zip(new,expected)) and time.monotonic()<until:
   time.sleep(.15);new=textedit_box(g)
  assert all(abs(a-b)<=1 for a,b in zip(new,expected)),(edge,old,new,expected)
  results['resizes'].append({'edge':edge,'before':old,'after':new})
 g.shot(environment+'-resized-textedit');g.key('meta_l-w');g.visible('Sessione locale');g.cmd('cat Documents/m7-body.txt','WiWi WWW iii caffè €')
 ok(environment+': actual TextEdit proportional F_BODY pixels, supplied pointer art/hotspots, intermediate window-move frames, all eight resize directions, original native UTF8 bytes retained')
 for app,label in [('Note','Tutte le note'),('Memo','Tutti i memo')]:
  launch(g,app,label);g.type('WiWi WWW iii àè €');g.visible('WiWi',faces=(1,));g.shot(environment+'-'+app.lower()+'-proportional');g.key('meta_l-w');time.sleep(.5)
 ok(environment+': actual Note and Memo document text uses proportional system face, not F_MONO')
 return results

def setup(g,ok):
 rows=[cursor(g,700,445,'Text.cur','m7-setup-text-cursor'),cursor(g,640,574,'Select.cur','m7-setup-hand-cursor')]
 g.move(50,700);ok('Live setup: original Text/Select cursor images and hotspots match framebuffer at editable field and real Continue button');return rows

def recovery(g,ok):
 g.cmd('ls -l "/System/Library/Cursors/Busy.ani"','128888')
 x,y=g.visible('Sessione locale',faces=(0,));gx,gy,_=geometry('Sessione locale',0);bx=x-gx-620;by=y-gy-419
 rows=[cursor(g,bx+300,by+180,'Text.cur','m7-recovery-text-cursor'),cursor(g,bx+400,by+22,'Normal.cur','m7-recovery-normal-cursor')]
 ok('Cold internal Recovery: actual filesystem Busy.ani size, original text/normal cursor pixels and hotspots used by native Terminal');return rows

def busy(g,ok):
 x,y=g.visible('Installazione in corso',faces=(3,),timeout=45);gx,gy,_=geometry('Installazione in corso',3);cx=x-gx-24;cy=y-gy-95
 raw=(ROOT/'assets/cursors/Busy.ani').read_bytes();frames=[]
 def walk(data):
  at=0
  while at+8<=len(data):
   tag=data[at:at+4];n=struct.unpack_from('<I',data,at+4)[0];v=data[at+8:at+8+n]
   if tag==b'LIST':walk(v[4:])
   elif tag==b'icon':
    hx,hy,sz,off=struct.unpack_from('<HHII',v,10);rgba=np.frombuffer(v,np.uint8,32*32*4,off+40).reshape(32,32,4)[::-1].copy();frames.append((hx,hy,rgba))
   at+=8+n+n%2
 walk(raw[12:]);assert len(frames)==30
 px,py=cx+400,cy+275;g.move(px,py);indices=[]
 for j in range(8):
  until=time.monotonic()+12;best=(0,None)
  while time.monotonic()<until:
   shot=np.asarray(g.snap()).astype(np.int32);best=(0,None)
   for k,(hx,hy,rgba) in enumerate(frames):
    patch=shot[py-hy:py-hy+32,px-hx:px-hx+32];mask=rgba[:,:,3]>=250;rgb=rgba[:,:,[2,1,0]].astype(np.int32);agreement=float(np.mean(np.max(abs(patch-rgb),axis=2)[mask]<=3))
    if agreement>best[0]:best=(agreement,k)
   if best[0]>.97:break
   time.sleep(.1)
  assert best[0]>.97,best;indices.append(best[1]);time.sleep(.11)
 assert len(set(indices))>=3,indices;g.shot('m7-live-busy-ani');ok('Live real installer: supplied Busy.ani renders multiple matched original frames on OS timer while installation proceeds');return indices
