"""Block 5: real installed guest, USB gestures and exact shipping-font pixels.
The sole offline fixture is a plain-text input document, added AFTER a real GUI
installation by apps5_telemetry_guest.py. No account/system/counter injection.
After guest save the document is extracted read-only and compared byte-for-byte.
"""
from pathlib import Path
import hashlib,json,os,struct,subprocess,time
import numpy as np
import pixels
from retail_guest import Retail
from guest import KEYS
qa=Path(os.environ['TMC_QA_DIR']);telemetry=json.loads((qa/'m3-4-apic-result.json').read_text());assert telemetry['passed']
w=Path(telemetry['work']);ev=w/'evidence';disk=w/'target/system.raw';root=Path(__file__).resolve().parents[1]
os.environ.update(TMC_MEM='512',TMC_SMP='4',TMC_POINTER='tablet',TMC_STORAGE='nvme')
r={'passed':False,'work':str(w),'iso_sha256':telemetry['iso_sha256'],'checks':[],'physical_hardware_tested':False,'document_only_offline_fixture':True,'pixel_frames':[]}
def save():(qa/'apps5-visual-result.json').write_text(json.dumps(r,indent=2)+'\n')
def ok(s):r['checks'].append(s);save();print('PASS',s,flush=True)
with disk.open('rb') as f:f.seek(1024+128+32);a,b=struct.unpack('<QQ',f.read(16))
exe=w/'apps5-fixture'
subprocess.run(['gcc','-O2','-fno-builtin','-I'+str(root/'include'),str(root/'tests/apps5_document_fixture.c'),str(root/'storage/tmcbfs2.c'),str(root/'crypto/sha256.c'),str(root/'kernel/string.c'),'-o',str(exe)],check=True)
if not os.environ.get('TMC_APPS5_FIXTURE_PREPARED'):subprocess.run([str(exe),str(disk),str(a),str(b-a+1),'prepare'],check=True)
original=subprocess.check_output([str(exe),str(disk),str(a),str(b-a+1),'read']);text=original.decode();r['document_sha256']=hashlib.sha256(original).hexdigest();r['document_bytes']=len(original)
expected_input=''.join(f"Riga {j:03d}: abcdefghijklmnopqrstuvwxyz fgijpq àèéòù € parole intere senza perdere lettere. LUNGHISSIMAPAROLASENZASPAZIABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 fine riga.\n" for j in range(48)).encode()
assert original==expected_input,'Input document differs from the deterministic text-only fixture'
class QA(Retail):
 initial='TelemetryQA'
 def type(self,text):
  for c in text:self.key(KEYS.get(c,('shift-'+c.lower() if c.isupper() else c)),dt=.055,hold=.025)
def launch(g,name,label):g.key('meta_l-spc');g.type(name);g.key('ret');g.visible(label);time.sleep(.5)
def origin(g,s,f):
 x,y=g.visible(s,faces=(f,));p=pixels.start+f*(4+pixels.ng*8);h,=struct.unpack_from('<I',pixels.b,p);mask=[];at=0
 for c in s:
  adv,ww,off=struct.unpack_from('<HHI',pixels.b,p+4+8*pixels.codes[ord(c)]);v=np.frombuffer(pixels.b,np.uint8,count=h*ww,offset=pixels.pix+off).reshape(h,ww);ys,xs=np.nonzero(v);mask.extend((int(xx)+at,int(yy)) for xx,yy in zip(xs,ys));at+=adv
 return x-max(0,min(q[0] for q in mask)-1),y-max(0,min(q[1] for q in mask)-1)
def client(g,aw):x,y=origin(g,'Testo semplice',0);return x-(aw-117),y-16
# Independent Unicode reference: word breaks retain whitespace, hard newlines
# are not drawn, overlong words split only between Unicode scalar values.
def rows_for(cols):
 starts=[0];i=col=0;br=-1
 while i<len(text):
  c=text[i];nxt=i+1
  if c=='\n':starts.append(nxt);i=nxt;col=0;br=-1;continue
  if c in ' \t':br=nxt
  col+=1
  if col>=cols and nxt<len(text) and text[nxt]!='\n':
   at=br if br>starts[-1] else nxt;starts.append(at);i=at;col=0;br=-1
  else:i=nxt
 return [text[x:y].removesuffix('\n') for x,y in zip(starts,starts[1:]+[len(text)])]
def alpha(s,width):
 out=np.zeros((20,width),np.uint16);p=pixels.start+5*(4+pixels.ng*8);h,=struct.unpack_from('<I',pixels.b,p)
 for j,c in enumerate(s):
  code=pixels.codes.get(ord(c),pixels.codes[ord('?')])
  _,ww,off=struct.unpack_from('<HHI',pixels.b,p+4+8*code)
  v=np.frombuffer(pixels.b,np.uint8,count=h*ww,offset=pixels.pix+off).reshape(h,ww)
  assert j*8+ww<=width
  out[:min(20,h),j*8:j*8+ww]=v[:20]
 return out
colors=None
def exact(g,aw,ah,first,label):
 global colors
 g.move(50,735);time.sleep(.4);im=g.snap();x,y=client(g,aw);arr=np.asarray(im);cols=max(1,(aw-56)//8);rows=rows_for(cols);nr=max(1,(ah-115)//20);sample=arr[y+70:y+70+nr*20,x+28:x+28+cols*8].astype(np.int32)
 masks=np.vstack([alpha(s,cols*8) for s in rows[first:first+nr]])
 assert len(masks)==len(sample),(label,len(masks),len(sample),first)
 if colors is None:
  bg=arr[y+70,x+10].astype(np.int32);fg=sample[masks==255][0];colors=(bg,fg)
 bg,fg=colors;al=masks[:,:,None].astype(np.int32);expected=np.where(al>=255,fg,(bg*(256-al)+fg*al)>>8)
 bad=np.any(sample!=expected,axis=2)
 # The insertion point was explicitly sent to EOF. At the final viewport it
 # may blink on: accept that EXACT additional 2x16 ACC rectangle, never mask
 # an area, ignore differing glyph pixels, or enlarge comparison tolerances.
 caret_on=False
 if np.any(bad) and first+nr==len(rows):
  with_caret=expected.copy();cy=(len(rows)-1-first)*20+1;cx=len(rows[-1])*8
  with_caret[cy:cy+16,cx:cx+2]=[0x52,0x6c,0x86] # shipping default I_ACC
  if np.array_equal(sample,with_caret):bad[:]=False;caret_on=True
 if np.any(bad):
  yy,xx=np.nonzero(bad);raise AssertionError((label,'exact glyph pixels differ',int(bad.sum()),[int(xx.min()),int(yy.min()),int(xx.max()),int(yy.max())],sample[yy[0],xx[0]].tolist(),expected[yy[0],xx[0]].tolist(),[x,y],first))
 im.save(ev/(label+'.png'));r['pixel_frames'].append({'label':label,'client':[x,y,aw,ah],'first_row':first,'visible_rows':nr,'columns':cols,'pixels_exact':int(bad.size),'exact_eof_caret_present':caret_on,'glyphs':sum(map(len,rows[first:first+nr]))});save();return x,y,rows,nr

def wheel(g,x,y,direction,n=1):
 g.move(x,y)
 for _ in range(n):
  for down in (True,False):g.req('input-send-event',events=[{'type':'btn','data':{'button':'wheel-'+direction,'down':down}}])
  time.sleep(.12)
try:
 with QA('/not-attached',w,ev,'apps5-native',disk='target',boot_disk=True) as g:
  g.unlock();g.key('meta_l-w');launch(g,'Terminale','Sessione locale');g.cmd('open Documents/apps5-input.txt');g.visible('Testo semplice');aw,ah=758,480
  # Put the caret outside the first viewport, then scroll without moving it.
  g.key('meta_l-down');x,y=client(g,aw);wheel(g,x+300,y+160,'up',100)
  x,y,rows,nr=exact(g,aw,ah,0,'apps5-textedit-top');ok('Native TextEdit: all visible glyph pixels equal the shipping TFN2 reference, with word/UTF-8/overlong-word wrapping; no clipped or omitted letters')
  wheel(g,x+300,y+160,'down',2);exact(g,aw,ah,6,'apps5-textedit-wheel');g.key('meta_l-c');exact(g,aw,ah,6,'apps5-textedit-copy-viewport')
  # Track click below the thumb pages the document, not its insertion point.
  g.click(x+aw-15,y+220);x,y,_,_=exact(g,aw,ah,6+nr,'apps5-textedit-track')
  # Return to the top and drag the real thumb to the bottom.
  wheel(g,x+300,y+160,'up',100);g.drag(x+aw-15,y+65,0,ah-115);x,y,rows,nr=exact(g,aw,ah,len(rows)-nr,'apps5-textedit-thumb-bottom')
  # Real bottom-right resize, two hundred narrower / one hundred shorter.
  g.drag(x+aw-3,y+ah-3,-200,-100);aw-=200;ah-=100;time.sleep(.6);x,y=client(g,aw);wheel(g,x+200,y+140,'up',100);exact(g,aw,ah,0,'apps5-textedit-narrow')
  ok('Real USB wheel scroll, Copy preserving viewport, track paging, thumb drag to final row and narrower/shorter window all retain exact visible font pixels')
  g.key('meta_l-up');g.key('end');g.type('X');g.key('backspace');g.key('meta_l-s');g.visible('Salvato su disco',faces=(0,));g.key('meta_l-w');g.visible('Sessione locale')
  launch(g,'Calcolatrice','Cronologia');g.type('7+5=');g.visible('7 + 5 = 12');g.move(50,735);g.shot('apps5-calculator-history');g.key('meta_l-w');g.visible('Sessione locale');g.cmd('off');g.p.wait(timeout=20)
 ok('Native TextEdit insert/delete/save and close; Calculator real keyboard result and history 7 + 5 = 12, separated glass; normal shutdown')
 result=subprocess.check_output([str(exe),str(disk),str(a),str(b-a+1),'read']);assert result==original
 ok('Read-only extraction after guest shutdown matches every original UTF-8 document byte after wrap/scroll/resize/edit/save')
 r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:
 save()
 for p in w.glob('*.ppm'):p.unlink()
