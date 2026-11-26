#!/usr/bin/env python3
"""Synthetic fixed-cell decoder regression; never prints a credential."""
from pathlib import Path
import ast,struct,cv2,numpy as np
from PIL import Image
import pixels
source=Path(__file__).with_name('encrypted_install_reset_guest.py')
module=ast.parse(source.read_text())
exec(compile(ast.Module(body=[n for n in module.body if isinstance(n,ast.FunctionDef) and n.name=='read_rendered_key'],type_ignores=[]),str(source),'exec'))
def glyph(c,face):
 at=pixels.start+face*(4+pixels.ng*8);h=struct.unpack_from('<I',pixels.b,at)[0];a,w,o=struct.unpack_from('<HHI',pixels.b,at+4+8*pixels.codes[ord(c)])
 return np.frombuffer(pixels.b,np.uint8,count=h*w,offset=pixels.pix+o).reshape(h,w),a
label='Conferma gli ultimi 8 caratteri della chiave:'
class Screen:
 def __init__(self,text,bg,fg):
  self.array=np.full((450,700),bg,dtype=np.uint8)
  def render(text,face,x,y):
   for c in text:
    mask,a=glyph(c,face);h,w=mask.shape;self.array[y:y+h,x:x+w]=np.rint(bg+(fg-bg)*mask.astype(float)/255).astype(np.uint8);x+=a
  render(label,0,70,280);render(text[:32],5,70,204);render(text[32:],5,70,231)
 def snap(self):return Image.fromarray(self.array).convert('RGB')
 def visible(self,text):
  hit=pixels.locate(self.snap(),text,faces=(0,));assert hit is not None;return hit[1]
rng=np.random.default_rng(125)
for uppercase in (False,True):
 alphabet='0123456789ABCDEF' if uppercase else '0123456789abcdef'
 text=''.join(rng.permutation(list(alphabet*4)))
 for bg,fg in ((244,30),(25,225)):
  screen=Screen(text,bg,fg);assert read_rendered_key(screen,uppercase)==text
  screen.array[204:225,70+10*8:70+11*8]=bg
  try:read_rendered_key(screen,uppercase)
  except AssertionError:pass
  else:raise AssertionError('Corrupt secret cell was not rejected')
print('PASS fixed-cell pixel decoder: all 16 hex symbols in both cases, light/dark contrast; corrupt cell refused. No secret logged.')
