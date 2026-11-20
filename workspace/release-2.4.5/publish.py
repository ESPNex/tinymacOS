#!/usr/bin/env python3
"""Publish ONLY the QA-gated production ISO via existing authorized GitHub CLI.
Creates a draft, independently downloads and checks both assets, then publishes.
Never publishes diagnostic images, credentials, a different repository or tag.
"""
import hashlib,json,os,subprocess,tempfile
from pathlib import Path
home=Path(__file__).resolve().parents[2];r=Path(__file__).resolve().parent
repo='ESPNex/tinymacOS';tag='v2.4.5';gh='/var/tmp/tmc-github-cli/gh'
env={**os.environ,'GH_CONFIG_DIR':'/var/tmp/tmc-github-auth'}
def run(*args):return subprocess.run(list(map(str,args)),cwd=home,env=env,check=True,stdout=subprocess.PIPE,text=True).stdout
def api(path):return json.loads(run(gh,'api',path))
a=json.loads((r/'artifact.json').read_text());qa=json.loads((r/'final-qa.json').read_text())
assert qa['passed'] and qa['kernel_runtime_regressions']=={'passed':29,'total':29,'production_iso':False}
assert qa['production_iso_sha256']==a['sha256'] and not a['qa_only'] and not a['browser']
iso=Path('/var/tmp/tmc245-final')/a['filename']
assert hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()==a['sha256'] and iso.stat().st_size==a['bytes']
assert (r/'SHA256SUMS.txt').read_text()==a['sha256']+'  '+iso.name+'\n'
assert api('repos/'+repo)['private']
sha=run('git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git','--work-tree='+str(home),'rev-parse','HEAD').strip()
assert api('repos/'+repo+'/commits/main')['sha']==sha,'Source backup must be pushed and verified first'
assert not run('git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git','--work-tree='+str(home),'status','--porcelain').strip(),'Commit source and QA before publishing'
# create refuses an existing release; no clobbering older assets or retagging
print(run(gh,'release','create',tag,iso,r/'SHA256SUMS.txt','--repo',repo,'--target',sha,'--title','tinymacOS 2.4.5 Levante — manutenzione input (21B6)','--notes-file',r/'RELEASE.md','--draft'),flush=True)
w=Path(tempfile.mkdtemp(prefix='tmc245-release-download-',dir='/var/tmp'))
run(gh,'release','download',tag,'--repo',repo,'--pattern',iso.name,'--pattern','SHA256SUMS.txt','--dir',w)
download=w/iso.name
assert download.stat().st_size==a['bytes'] and hashlib.file_digest(download.open('rb'),'sha256').hexdigest()==a['sha256']
assert (w/'SHA256SUMS.txt').read_bytes()==(r/'SHA256SUMS.txt').read_bytes()
ref=api('repos/'+repo+'/git/ref/tags/'+tag)
assert ref['object']['type']=='commit' and ref['object']['sha']==sha,'Release tag source mismatch'
run(gh,'release','edit',tag,'--repo',repo,'--draft=false','--latest')
j=json.loads(run(gh,'release','view',tag,'--repo',repo,'--json','url,isDraft,assets,tagName,targetCommitish'))
assert not j['isDraft'] and j['tagName']==tag
assets={x['name']:x for x in j['assets']};assert assets[iso.name]['size']==a['bytes']
record={'published':True,'url':j['url'],'tag':tag,'tag_commit':sha,'iso_sha256':a['sha256'],'bytes':a['bytes'],'remote_download_verified':True,'downloaded_external':str(w),'assets':[{'name':x['name'],'size':x['size'],'url':x['url']} for x in j['assets']]}
(r/'github-release-verification.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps(record,indent=2),flush=True)
