#!/usr/bin/env python3
"""Source-first publication; new 10.1.1 Latest, archive unchanged 10.1 and 10 as prereleases.
Never logs credentials, replaces old ISO bytes or force-updates an existing tag.
"""
import argparse,hashlib,json,os,subprocess,tempfile
from pathlib import Path
q=Path(__file__).resolve().parent;home=q.parents[1];repo='ESPNex/tinymacOS'
gh='/var/tmp/tmc-github-cli/gh';env={**os.environ,'GH_CONFIG_DIR':'/var/tmp/tmc-github-auth','GODEBUG':'http2client=0'}
ap=argparse.ArgumentParser();ap.add_argument('--source-commit',required=True);args=ap.parse_args()
def run(a,**kw):return subprocess.run([gh,*a],env=env,check=True,**kw)
def api(path,data=None):
 a=['api','repos/'+repo+('/'+path if path else '')]
 if data is not None:a+=['--method','PATCH','--input','-']
 return json.loads(run(a,input=json.dumps(data).encode() if data is not None else None,stdout=subprocess.PIPE).stdout)
def sha(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
qa=json.loads((q/'final-qa.json').read_text());assert qa['passed']
assert api('')['private'] is True
source=api('commits/main')['sha'];assert source==args.source_commit
assert source==subprocess.check_output(['git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git','rev-parse','HEAD'],text=True).strip()
assert not subprocess.check_output(['git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git','--work-tree='+str(home),'status','--porcelain'],text=True).strip()
# Validate EVERY frozen OS source against the committed tree, not just HEAD text.
tree=subprocess.check_output(['git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git','ls-tree','-rz',source,'tinymacOS/'])
blobs={}
for item in tree.split(b'\0'):
 if item:
  meta,path=item.split(b'\t',1);blobs[path.decode()]=meta.split()[2].decode()
for name,h in json.loads((q/'source-hashes.json').read_text()).items():
 p=home/'tinymacOS'/name;data=p.read_bytes();assert hashlib.sha256(data).hexdigest()==h,name
 assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==blobs['tinymacOS/'+name],name
assert not subprocess.check_output(['git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git','ls-tree','-r','--name-only',source,'private-backup/'])
release_list=api('releases?per_page=100');before={r['tag_name']:r for r in release_list}
assert 'v10' in before
newiso=Path('/var/tmp/tmc-10.1.1')/qa['filename'];assert sha(newiso)==qa['sha256']
oldq=home/'workspace/release-10.1-30B1';oldqa=json.loads((oldq/'final-qa.json').read_text());oldiso=Path('/var/tmp/tmc-10.1')/oldqa['filename'];assert oldqa['passed'] and sha(oldiso)==oldqa['sha256']
oldsource='0965f921147062721fed7e7adea9a750cd0beceb'
notesold=Path('/var/tmp/tmc-10.1.1/archive-10.1-notes.md');notesold.write_text('''# tinymacOS 10.1 Tramontana · build 30B1\n\nArchivio **Pre-release** della ISO 10.1 già compilata e collaudata. Stessi byte e sorgenti al commit `'''+oldsource+'''`. Non contiene le correzioni mouse/Recovery della 10.1.1. Usare la [10.1.1](https://github.com/ESPNex/tinymacOS/releases/tag/v10.1.1) per la nuova manutenzione.\n\nCentro di Controllo modulare in vetro, comandi reali, cursori tema/sfondo. Non è macOS Apple; Safari/WebKit non incluso. Il problema del mouse sul PC fisico è segnalato dall’utente: i test QEMU non certificano quell’hardware.\n\nSHA-256: `'''+oldqa['sha256']+'`\n')
work=Path(tempfile.mkdtemp(prefix='tmc1011-publish-',dir='/var/tmp'));verified=[]
for tag,ver,iso,checksum,commit,notes,pre in [
 ('v10.1','10.1',oldiso,oldq/'SHA256SUMS.txt',oldsource,notesold,True),
 ('v10.1.1','10.1.1',newiso,q/'SHA256SUMS.txt',source,q/'RELEASE.md',False)]:
 expected=sha(iso);assert checksum.read_text().strip()==expected+'  '+iso.name
 current=next((r for r in api('releases?per_page=100') if r['tag_name']==tag),None)
 if not current:
  cmd=['release','create',tag,'--repo',repo,'--target',commit,'--title','tinymacOS '+ver+' Tramontana','--notes-file',str(notes),'--draft']
  if pre:cmd+=['--prerelease']
  run(cmd)
 current=next(r for r in api('releases?per_page=100') if r['tag_name']==tag)
 # An unexpected existing published release must never be overwritten by retries.
 if not current['draft']:
  tagref=api('git/ref/tags/'+tag)['object'];assert tagref['sha']==commit,(tag,tagref)
 for p in [checksum,iso]:
  found=next((a for a in current['assets'] if a['name']==p.name),None)
  if found:
   assert found['size']==p.stat().st_size and found.get('digest')=='sha256:'+sha(p),(tag,p.name)
  else:run(['release','upload',tag,str(p),'--repo',repo])
 download=work/tag;download.mkdir()
 run(['release','download',tag,'--repo',repo,'--dir',str(download),'--pattern',iso.name,'--pattern','SHA256SUMS.txt'])
 assert sha(download/iso.name)==expected and (download/'SHA256SUMS.txt').read_bytes()==checksum.read_bytes()
 current=api('releases/'+str(current['id']),{'draft':False,'prerelease':pre,'make_latest':'false' if pre else 'true'})
 ref=api('git/ref/tags/'+tag)['object'];assert ref['sha']==commit,(tag,ref)
 assert len(current['assets'])==2
 for a in current['assets']:
  p=iso if a['name']==iso.name else checksum;assert a['size']==p.stat().st_size and a['digest']=='sha256:'+sha(p)
 verified.append({'tag':tag,'id':current['id'],'commit':commit,'prerelease':pre,'bytes':iso.stat().st_size,'filename':iso.name,'sha256':expected,'download_verified':True,'assets':[{k:a.get(k) for k in ['id','name','size','digest','browser_download_url']}for a in current['assets']]})
 print('PASS uploaded and independently downloaded',tag,expected,flush=True)
# Preserve every existing asset; only distribution status/body note may change.
for r in api('releases?per_page=100'):
 if r['tag_name']=='v10.1.1':continue
 if not r['prerelease']:
  patch={'prerelease':True,'make_latest':'false'}
  if r['tag_name']=='v10':
   prefix='> Archivio: questa versione è ora **Pre-release**. La release normale/Latest è [10.1.1 Tramontana](https://github.com/ESPNex/tinymacOS/releases/tag/v10.1.1). Le note sottostanti descrivono la pubblicazione storica 30A4.\n\n'
   patch['body']=prefix+r['body']
  api('releases/'+str(r['id']),patch)
latest=api('releases/latest');assert latest['tag_name']=='v10.1.1' and not latest['draft'] and not latest['prerelease']
after=api('releases?per_page=100');assert all(r['prerelease'] for r in after if r['tag_name']!='v10.1.1')
for tag,r in before.items():
 if tag in ['v10.1','v10.1.1']:continue
 now=next(x for x in after if x['id']==r['id'])
 def assets(x):return sorted((a['id'],a['name'],a['size'],a.get('digest'))for a in x['assets'])
 assert assets(r)==assets(now),(tag,'historical ISO/assets changed')
record={'passed':True,'latest':latest['html_url'],'source_commit':source,'source_synced_before_publication':True,'verified_releases':verified,'historical_assets_unchanged':True,'all_older_prerelease':True,'total_releases':len(after),'external_download_dir':str(work),'credentials_or_key_committed':False,'physical_hardware_tested':False}
(q/'github-release-verification.json').write_text(json.dumps(record,indent=2)+'\n');print('PASS final publication',latest['html_url'],flush=True)
