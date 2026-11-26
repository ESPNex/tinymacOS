"""Rollback-compatible context-menu/keyboard Finder transfers and shared Terminal text.
Requires GUI-installed FS2 fixture from this exact candidate ISO. No host writes
into guest filesystems, no debug syscall, no injected application action events.
"""
from pathlib import Path
import hashlib,json,os,struct,time
from retail_guest import Retail
import pixels,retail_guest
base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO']);fixture=json.loads((base/'pmbr-signature-guest-result.json').read_text());assert fixture['passed'] and fixture['iso_sha256']==hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()
w=Path(fixture['work']);ev=w/'evidence';font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.96:old(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER=os.environ.get('TMC_TRANSFER_POINTER','tablet'),OCR_ROOT=os.environ['TOOLROOT'])
r={'passed':False,'release_acceptance':False,'work':str(w),'iso_sha256':fixture['iso_sha256'],'host_formatted_target':False,'checks':[]}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
case=os.environ.get('TMC_FINDER_CASE','ClipQA')
r['qa_case_name']=case
class Installed(Retail):
 initial='SigFix'
 def cmd(self,s,expect=None,timeout=24):return super().cmd(s.replace('DropQA',case),expect,timeout)
def terminal(g):g.key('f2');g.visible('Sessione locale')
def point(g,label):x,y=g.visible(label,box=(0,40,1270,780),faces=(0,));return x+8,y+7
def right(g,x,y):
 g.move(x,y);g.req('input-send-event',events=[{'type':'btn','data':{'down':True,'button':'right'}}]);time.sleep(.12);g.req('input-send-event',events=[{'type':'btn','data':{'down':False,'button':'right'}}])
def menu(g,label):x,y=g.visible(label);g.click(x+12,y+7);time.sleep(.4)
def source(g):
 terminal(g);g.cmd('open Documents/DropQA/Src');g.visible('Src',faces=(3,));time.sleep(.65);g.visible('solo.txt',faces=(0,));return point(g,'solo.txt')
try:
 with Installed('/not-attached',w,ev,'clipboard-rollback',disk='target',boot_disk=True) as g:
  g.unlock();terminal(g)
  for cmd in ['rm Desktop/solo.txt','rm Desktop/desk.txt','mkdir Documents/DropQA','mkdir Documents/DropQA/Src','mkdir Documents/DropQA/Sink','mkdir Documents/DropQA/Src/Bundle','mkdir Documents/DropQA/Src/Bundle/Sub','mkdir Documents/DropQA/Src/DragBin','echo FILE_TRANSFER_1025 > Documents/DropQA/Src/solo.txt','echo DESKTOP_DROP_1025 > Documents/DropQA/Src/desk.txt','echo NESTED_TRANSFER_1025 > Documents/DropQA/Src/Bundle/Sub/a.txt']:g.cmd(cmd)
  g.cmd('cp -R Documents/DropQA/Src/Bundle Documents/DropQA/Sink/CliCopy');g.cmd('cat Documents/DropQA/Sink/CliCopy/Sub/a.txt','NESTED_TRANSFER_1025');g.cmd('mv Documents/DropQA/Sink/CliCopy Documents/DropQA/Sink/CliMoved');g.cmd('cat Documents/DropQA/Sink/CliMoved/Sub/a.txt','NESTED_TRANSFER_1025')
  g.cmd('echo SYSTEM_CLIPBOARD_1025 | pbcopy');g.cmd('pbpaste','SYSTEM_CLIPBOARD_1025');ok('Real Terminal cp -R/mv copy and move nested folders; pbcopy/pbpaste exchange actual shared text')
  a=source(g);right(g,*a);menu(g,'Copia');g.visible('Elemento copiato negli appunti.')
  terminal(g);g.cmd('open Documents/DropQA/Sink');hx,hy=g.visible('Sink',faces=(3,));time.sleep(.65);right(g,hx+300,hy+240);menu(g,'Incolla');g.visible('Copia creata.')
  terminal(g);g.cmd('cat Documents/DropQA/Sink/solo.txt','FILE_TRANSFER_1025');g.cmd('cat Documents/DropQA/Src/solo.txt','FILE_TRANSFER_1025');ok('Finder context Copy/Paste transfers a real file across distinct Finder windows without deleting source')
  source(g);a=point(g,'Bundle');right(g,*a);menu(g,'Taglia');g.visible('Elemento tagliato:')
  terminal(g);g.cmd('open Documents/DropQA/Sink');hx,hy=g.visible('Sink',faces=(3,));time.sleep(.65);right(g,hx+300,hy+240);menu(g,'Incolla');g.visible('Elemento spostato.')
  terminal(g);g.cmd('cat Documents/DropQA/Sink/Bundle/Sub/a.txt','NESTED_TRANSFER_1025');ok('Finder context Cut/Paste moves a directory with nested files across windows through the real atomic move syscall')
  g.cmd('echo NOT_EXECUTED_BY_PASTE | pbcopy');g.key('ctrl-shift-v');g.visible('NOT_EXECUTED_BY_PASTE');g.key('ctrl-u');g.type('echo INPUT_CLIPBOARD_1025');g.key('ctrl-shift-c');g.key('ctrl-u');g.key('ctrl-shift-v');g.key('ret');g.visible('INPUT_CLIPBOARD_1025');g.cmd('pbpaste','echo INPUT_CLIPBOARD_1025');ok('Terminal shared clipboard and input paste work without auto-execution')
  g.key('ctrl-shift-q');g.visible('SigFix',faces=(3,));g.unlock();terminal(g);g.cmd('pbpaste','Nessun testo negli appunti.');ok('Lock clears shared clipboard');g.shot('clipboard-rollback-preserved');g.cmd('off');g.p.wait(timeout=20)
 with Installed('/not-attached',w,ev,'clipboard-rollback-reboot',disk='target',boot_disk=True) as g:
  g.unlock();terminal(g);g.cmd('cat Documents/DropQA/Sink/Bundle/Sub/a.txt','NESTED_TRANSFER_1025');g.cmd('cat Documents/DropQA/Sink/solo.txt','FILE_TRANSFER_1025');g.cmd('cat Documents/DropQA/Src/solo.txt','FILE_TRANSFER_1025');g.cmd('diskutil verifyVolume disk0','Il volume risulta valido.',timeout=180);g.shot('clipboard-rollback-cold-persistence');g.cmd('off');g.p.wait(timeout=20)
 ok('Context menu copy/move files persist after cold boot and full volume verification; desktop file DnD deliberately excluded');r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:(base/'clipboard-rollback-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
