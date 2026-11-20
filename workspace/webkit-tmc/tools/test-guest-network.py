#!/usr/bin/env python3
"""Actual target backend -> syscall -> lwIP -> e1000 -> QEMU slirp -> local peer.
No public network, no debug serial success criterion, no production ISO upload.
Requires tools/build-guest-network-probe.sh and baseline toolchain in /var/tmp.
"""
import hashlib,json,os,socket,struct,sys,threading,time,traceback,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OS=ROOT.parent.parent/'tinymacOS'
sys.path.insert(0,str(OS/'tests'))
from retail_guest import Retail
if os.environ.get('TMC_PROBE_RUN'):
 WORK=Path(os.environ['TMC_PROBE_RUN']);WORK.mkdir(parents=True,exist_ok=False)
else:WORK=Path(tempfile.mkdtemp(prefix='tmc-webkit-network-',dir='/var/tmp'))
cpus=int(os.environ.get('TMC_SMP','2'));assert cpus in (1,2),'Test profiles are 1 or 2 vCPUs'
EVIDENCE=ROOT/('test-results/guest-network-smp'+str(cpus));EVIDENCE.mkdir(exist_ok=True)
IMAGE=Path(os.environ.get('TMC_PROBE_BUILD','/var/tmp/tmc-webkit-network-guest'))/'tinymacOS-2.4.5-x86_64.iso'
BODY=bytes((i*37+(i>>8)*11+17)&255 for i in range(1048893))
DIGEST=hashlib.sha256(BODY).hexdigest()
records={'scope':'actual QEMU guest with target backend, lwIP/e1000/slirp and controlled loopback peer; NOT WebKit/HTTPS acceptance',
         'iso_qa_only':True,'iso_sha256':hashlib.sha256(IMAGE.read_bytes()).hexdigest(),
         'requested_vcpus':cpus,'font_bytes':2215962,'font_sha256':'ca99fceb218091816dfbc0f2dfaef7fef846599806d620c3f2c6302c1d1d0b8c','payload_bytes':len(BODY),'expected_sha256':DIGEST,'server':[], 'passed':False}
errors=[];done=threading.Event()
listener=socket.socket();listener.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
listener.bind(('127.0.0.1',18791));listener.listen(4);listener.settimeout(100)
def line(c,maximum=128):
 data=b''
 while not data.endswith(b'\n') and len(data)<maximum:
  b=c.recv(1)
  if not b:break
  data+=b
 return data

def peer():
 try:
  for wanted in (b'STREAM\n',b'WAIT\n',b'RESET\n'):
   c,addr=listener.accept();c.settimeout(60)
   with c:
    request=line(c);assert request==wanted,(wanted,request)
    r={'request':request.decode().strip()};records['server'].append(r)
    if wanted==b'STREAM\n':
     # Deliberately fragmented writes and >128x native ring capacity.
     at=0;pattern=(1,4093,65521,8197,307)
     while at<len(BODY):
      n=pattern[(at//307)%len(pattern)];c.sendall(BODY[at:at+n]);at+=min(n,len(BODY)-at)
     c.shutdown(socket.SHUT_WR);r['server_FIN_sent']=True
     ack=line(c);r['guest_digest_after_FIN']=ack.decode(errors='replace').strip()
     assert ack==(DIGEST+'\n').encode(),r
     r['payload_and_half_close_verified']=True
    elif wanted==b'WAIT\n':
     # Never send payload. Client must time out and see EAGAIN, then close.
     assert c.recv(1)==b'', 'Expected guest close without payload'
     r['guest_closed_without_data']=True
    else:
     c.setsockopt(socket.SOL_SOCKET,socket.SO_LINGER,struct.pack('ii',1,0));r['abortive_close_requested']=True
 except Exception:
  errors.append(traceback.format_exc())
 finally:
  listener.close();done.set()
t=threading.Thread(target=peer,daemon=True);t.start()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD=str(IMAGE.parent),TMC_NET='1',TMC_SMP=str(cpus))
try:
 with Retail(IMAGE,WORK,EVIDENCE,'network-probe',disk='vars',live=True) as g:
  records['qmp_vcpu_count']=len(g.req('query-cpus-fast'))
  assert records['qmp_vcpu_count']==cpus
  records['network_devices']=g.hmp('info network')
  g.enroll(name='Webkit')
  g.key('f2');g.visible('Sessione locale',15)
  g.cmd('/usr/bin/fetch')
  deadline=time.monotonic()+70
  from pixels import locate
  while time.monotonic()<deadline:
   im=g.snap()
   passed=locate(im,'WKNET PASS',faces=(0,1,2,3,5,6,8))
   failed=locate(im,'WKNET FAIL',faces=(0,1,2,3,5,6,8))
   if passed or failed:break
   time.sleep(.2)
  else:raise AssertionError('No probe result appeared in framebuffer')
  g.shot('guest-network-result')
  assert passed,'Probe reported WKNET FAIL; inspect guest-network-result.png'
  for marker in ('STREAM PASS bytes=1048893','WAIT PASS','RESET PASS ECONNRESET','FILE PASS bytes=2215962'):
   assert locate(im,marker,faces=(0,1,2,3,5,6,8)),marker
  records['framebuffer_test_markers_verified']=True
  assert done.wait(5),'Peer server did not complete'
  assert not errors,errors
  assert len(records['server'])==3,records
  records['passed']=True
  print('PASS: actual guest TCP and read-only file descriptors, SHA256, dup/shared seek, FIN/half-close, timeout/EAGAIN, invalid handles and ECONNRESET.')
except Exception:
 records['failure']=traceback.format_exc();raise
finally:
 records['server_errors']=errors
 (EVIDENCE/'result.json').write_text(json.dumps(records,indent=2))
