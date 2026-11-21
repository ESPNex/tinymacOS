#!/usr/bin/env python3
"""Publish only 10.1.3 after QA and source sync. Never rewrite older releases/tags."""
import argparse,hashlib,json,os,subprocess,tempfile,time,urllib.request
from pathlib import Path
q=Path(__file__).resolve().parent;home=q.parents[1];repo='ESPNex/tinymacOS';git=['git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git'];gh='/var/tmp/tmc-github-cli/gh'
env={**os.environ,'GH_CONFIG_DIR':'/var/tmp/tmc-github-auth','GODEBUG':'http2client=0'}
ap=argparse.ArgumentParser();ap.add_argument('--source-commit',required=True);args=ap.parse_args()
def run(a,**kw):return subprocess.run([gh,*a],env=env,check=True,**kw)
def api(path='',data=None):
 a=['api','repos/'+repo+('/'+path if path else '')]
 if data is not None:a+=['--method','PATCH','--input','-']
 return json.loads(run(a,input=json.dumps(data).encode() if data is not None else None,stdout=subprocess.PIPE).stdout)
def sha(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
def release(required=False):
 for attempt in range(6 if required else 1):
  r=next((r for r in api('releases?per_page=100') if r['tag_name']=='v10.1.3'),None)
  if r:return r
  if required:time.sleep(min(2**attempt,8))
 if required:raise RuntimeError('Draft creation not visible after bounded retries')
 return None
qa=json.loads((q/'final-qa.json').read_text());assert qa['passed'] and not qa['physical_hardware_tested']
assert api()['private'] is False
source=api('commits/main')['sha'];assert source==args.source_commit
assert subprocess.check_output(git+['rev-parse','HEAD'],text=True).strip()==source
assert not subprocess.check_output(git+['--work-tree='+str(home),'status','--porcelain'],text=True).strip()
tree=subprocess.check_output(git+['ls-tree','-rz',source,'tinymacOS/']);blobs={}
for item in tree.split(b'\0'):
 if item:
  meta,path=item.split(b'\t',1);blobs[path.decode()]=meta.split()[2].decode()
frozen=json.loads((q/'source-hashes.json').read_text());assert len(frozen)==575
for name,h in frozen.items():
 data=(home/'tinymacOS'/name).read_bytes();assert hashlib.sha256(data).hexdigest()==h,name
 assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==blobs['tinymacOS/'+name],name
for name,h in qa['result_files_sha256'].items():assert sha(q/name)==h,name
assert not subprocess.check_output(git+['ls-tree','-r','--name-only',source,'private-backup/'])
iso=Path('/var/tmp/tmc-10.1.3')/qa['filename'];checksum=q/'SHA256SUMS.txt'
assert sha(iso)==qa['sha256'] and iso.stat().st_size==qa['bytes']
assert checksum.read_text().strip()==qa['sha256']+'  '+iso.name
before=api('releases?per_page=100');tags_before={t['name']:t['commit']['sha'] for t in api('tags?per_page=100')}
assert all(r['prerelease'] for r in before if not r['draft'] and r['tag_name']!='v10.1.3')
r=release()
if r is None:
 run(['release','create','v10.1.3','--repo',repo,'--target',source,'--title','tinymacOS 10.1.3 Tramontana','--notes-file',str(q/'RELEASE.md'),'--draft'])
 r=release(True)
assert r['target_commitish']==source,(r['target_commitish'],source)
if not r['draft']:assert api('git/ref/tags/v10.1.3')['object']['sha']==source
for p in [checksum,iso]:
 found=next((a for a in r['assets'] if a['name']==p.name),None)
 if found:assert found['size']==p.stat().st_size and found.get('digest')=='sha256:'+sha(p)
 else:run(['release','upload','v10.1.3',str(p),'--repo',repo])
w=Path(tempfile.mkdtemp(prefix='tmc1012-publication-',dir='/var/tmp'))
run(['release','download','v10.1.3','--repo',repo,'--dir',str(w),'--pattern',iso.name,'--pattern','SHA256SUMS.txt'])
assert sha(w/iso.name)==qa['sha256'] and (w/'SHA256SUMS.txt').read_bytes()==checksum.read_bytes()
r=api('releases/'+str(r['id']),{'draft':False,'prerelease':False,'make_latest':'true'})
assert len(r['assets'])==2
for a in r['assets']:
 p=iso if a['name']==iso.name else checksum
 assert a['size']==p.stat().st_size and a['digest']=='sha256:'+sha(p)
assert api('git/ref/tags/v10.1.3')['object']['sha']==source
latest=api('releases/latest');assert latest['tag_name']=='v10.1.3' and not latest['draft'] and not latest['prerelease']
after=api('releases?per_page=100');tags_after={t['name']:t['commit']['sha'] for t in api('tags?per_page=100')}
assert all(tags_after[t]==h for t,h in tags_before.items())
for old in before:
 if old['tag_name']=='v10.1.3':continue
 new=next(x for x in after if x['id']==old['id'])
 def assets(r):return sorted((a['id'],a['name'],a['size'],a.get('digest')) for a in r['assets'])
 assert assets(old)==assets(new) and old['prerelease']==new['prerelease'] and old['body']==new['body']
# Truly anonymous full download: never attach a token/header/CLI credential.
a=next(a for a in r['assets'] if a['name']==iso.name)
req=urllib.request.Request(a['browser_download_url'],headers={'User-Agent':'tinymacOS-public-integrity-check'})
h=hashlib.sha256();n=0
with urllib.request.urlopen(req,timeout=90) as response:
 while data:=response.read(1024*1024):h.update(data);n+=len(data)
assert n==iso.stat().st_size and h.hexdigest()==qa['sha256']
record={'passed':True,'latest':r['html_url'],'release_id':r['id'],'tag':'v10.1.3','source_commit':source,'source_synced_before_publication':True,'filename':iso.name,'bytes':n,'sha256':h.hexdigest(),'download_verified':True,'anonymous_full_download_verified':True,'public_repository':True,'prerelease':False,'historical_releases_assets_and_tags_unchanged':True,'previous_10_1_1_kept_unchanged':True,'all_previous_releases_prerelease':True,'assets':[{k:a.get(k) for k in ['id','name','size','digest','browser_download_url']} for a in r['assets']],'credentials_or_key_committed':False,'physical_hardware_tested':False}
(q/'github-release-verification.json').write_text(json.dumps(record,indent=2)+'\n');print('PASS source-first public publication, independent anonymous full download, historical releases unchanged:',r['html_url'],flush=True)
