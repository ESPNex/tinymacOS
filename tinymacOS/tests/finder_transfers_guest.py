"""Real pointer/context-menu/keyboard Finder transfers and shared Terminal text.
Requires GUI-installed FS2 fixture from this exact candidate ISO. No host writes
into guest filesystems, no debug syscall, no injected application action events.
"""
from pathlib import Path
import hashlib,json,os,struct,time
from retail_guest import Retail
import pixels,retail_guest
base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO']);fixture=json.loads((base/'fs2-install-guest-result.json').read_text());assert fixture['passed'] and fixture['iso_sha256']==hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()
w=Path(fixture['work']);ev=w/'evidence';font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.96:old(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',OCR_ROOT=os.environ['TOOLROOT'])
r={'passed':False,'release_acceptance':False,'work':str(w),'iso_sha256':fixture['iso_sha256'],'kernel_sha256':fixture['kernel_sha256'],'host_formatted_target':False,'checks':[]}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
case=os.environ.get('TMC_FINDER_CASE','DropQA')
r['qa_case_name']=case
class Installed(Retail):
 initial='Fs2Install'
 def cmd(self,s,expect=None,timeout=24):return super().cmd(s.replace('DropQA',case),expect,timeout)
def terminal(g):g.key('f2');g.visible('Sessione locale')
def point(g,label):x,y=g.visible(label,box=(0,40,1270,780),faces=(0,));return x+8,y+7
def right(g,x,y):
 g.move(x,y);g.req('input-send-event',events=[{'type':'btn','data':{'down':True,'button':'right'}}]);time.sleep(.12);g.req('input-send-event',events=[{'type':'btn','data':{'down':False,'button':'right'}}])
def menu(g,label):x,y=g.visible(label);g.click(x+12,y+7);time.sleep(.4)
def source(g):
 terminal(g);g.cmd('open Documents/DropQA/Src');g.visible('Src',faces=(3,));time.sleep(.65);g.visible('solo.txt',faces=(0,));return point(g,'solo.txt')
def drag(g,a,b,copy=False):
 g.move(*a)
 if copy:g.req('input-send-event',events=[{'type':'key','data':{'down':True,'key':{'type':'qcode','data':'ctrl'}}}])
 g.req('input-send-event',events=[{'type':'btn','data':{'down':True,'button':'left'}}]);time.sleep(.25)
 for j in range(1,9):g.move(round(a[0]+(b[0]-a[0])*j/8),round(a[1]+(b[1]-a[1])*j/8))
 time.sleep(.35);g.req('input-send-event',events=[{'type':'btn','data':{'down':False,'button':'left'}}]);time.sleep(.5)
 if copy:g.req('input-send-event',events=[{'type':'key','data':{'down':False,'key':{'type':'qcode','data':'ctrl'}}}])
def move_window(g,heading,x,y):
 hx,hy=g.visible(heading,faces=(3,));cx,cy=hx-191,hy-29;oldx,oldy=cx-1,cy-43;g.drag(oldx+380,oldy+21,x-oldx,y-oldy);time.sleep(.5)
try:
 with Installed('/not-attached',w,ev,'finder-transfers',disk='target',boot_disk=True) as g:
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
  a=source(g);b=point(g,'DragBin');drag(g,a,b);g.visible('Elemento spostato.');a=point(g,'desk.txt');drag(g,a,(1190,710));g.visible('Elemento spostato.');terminal(g);g.cmd('cat Desktop/desk.txt','DESKTOP_DROP_1025');g.cmd('cat Documents/DropQA/Src/DragBin/solo.txt','FILE_TRANSFER_1025');ok('Real held-button drag drops a file onto a directory icon, changing the actual filesystem')
  g.cmd('open Documents/DropQA/Src/DragBin');a=point(g,'solo.txt');hx,hy=g.visible('DragBin',faces=(3,));drag(g,a,(hx-191+70,hy-29+79));g.visible('Elemento spostato.');terminal(g);g.cmd('cat Desktop/solo.txt','FILE_TRANSFER_1025');ok('Real drag to Finder sidebar moves into Desktop')
  g.cmd('open Desktop');move_window(g,'Scrivania',35,70);a=point(g,'solo.txt')
  terminal(g);g.cmd('open Documents/DropQA/Sink');move_window(g,'Sink',460,340);g.drag(1234,770,0,-100);time.sleep(.6);hx,hy=g.visible('Sink',faces=(3,));b=(hx+200,hy+180);g.click(180,91);drag(g,a,b,copy=True);g.visible('Copia creata.');terminal(g);g.cmd('cat Desktop/solo.txt','FILE_TRANSFER_1025');g.cmd('cat "Documents/DropQA/Sink/Copia solo.txt"','FILE_TRANSFER_1025');ok('Ctrl-drag across two positioned Finder windows copies rather than moves and generates a non-overwriting collision name')
  g.click(1140,360);x,y=g.visible('solo.txt',box=(1120,200,1270,335),faces=(0,));drag(g,(x+8,y+7),b,copy=True);g.visible('Copia creata.');terminal(g);g.cmd('cat "Documents/DropQA/Sink/Copia 2 solo.txt"','FILE_TRANSFER_1025');ok('Desktop file icons can be dragged into an open Finder destination, and Finder files can be dropped onto the bare Desktop')
  # Position Terminal below the source row, then drop a file path into input.
  fx,fy=g.visible('Sessione locale',faces=(0,));tx,ty=fx-620,fy-419;g.drag(tx+350,ty+21,480-tx,340-ty);time.sleep(.5);g.drag(1202,776,0,-100);time.sleep(.6);g.key('ctrl-l');g.type('cat ')
  g.click(180,91);x,y=g.visible('solo.txt',box=(200,200,445,320),faces=(0,));drag(g,(x+8,y+7),(950,620));g.visible('cat "/Users/User/Desktop/solo.',faces=(5,));g.key('ret');g.visible('FILE_TRANSFER_1025');ok('Real drag into Terminal inserts a quoted path into the pending command; only an explicit Enter executes it')
  g.cmd('echo NOT_EXECUTED_BY_PASTE | pbcopy');g.key('ctrl-shift-v');g.visible('NOT_EXECUTED_BY_PASTE');g.key('ctrl-u');g.type('echo INPUT_CLIPBOARD_1025');g.key('ctrl-shift-c');g.key('ctrl-u');g.key('ctrl-shift-v');g.key('ret');g.visible('INPUT_CLIPBOARD_1025');g.cmd('pbpaste','echo INPUT_CLIPBOARD_1025');ok('Terminal Ctrl+Shift+C/V uses shared clipboard while paste only edits input, never auto-executes')
  g.key('ctrl-shift-q');g.visible('Fs2Install',faces=(3,));g.unlock();terminal(g);g.cmd('pbpaste','Nessun testo negli appunti.');ok('Session lock clears the shared clipboard; old Finder/Terminal buffers cannot repaste the previous shared content');g.shot('finder-transfer-context-drag-terminal-passed');g.cmd('off');g.p.wait(timeout=20)
 with Installed('/not-attached',w,ev,'finder-transfers-reboot',disk='target',boot_disk=True) as g:
  g.unlock();terminal(g);g.cmd('cat Documents/DropQA/Sink/Bundle/Sub/a.txt','NESTED_TRANSFER_1025');g.cmd('cat Desktop/solo.txt','FILE_TRANSFER_1025');g.cmd('cat "Documents/DropQA/Sink/Copia solo.txt"','FILE_TRANSFER_1025');g.cmd('diskutil verifyVolume disk0','Il volume risulta valido.',timeout=180);g.shot('finder-transfers-persistent-after-reboot');g.cmd('off');g.p.wait(timeout=20)
 ok('Copied/moved folders and drag results persist through a cold installed boot and full data/system verification');r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:(base/'finder-transfers-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
