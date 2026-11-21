#!/usr/bin/env python3
"""Resume 30A1 after GitHub returned 404 for by-tag lookup of an uploaded draft.
Discover the draft by release listing, validate its tag/source/assets and download
before publishing. No recreation, overwrite, retag or asset deletion.
"""
import hashlib,json,os,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parent;home=r.parents[1];repo='ESPNex/tinymacOS';tag='v10.0';gh='/var/tmp/tmc-github-cli/gh';env={**os.environ,'GH_CONFIG_DIR':'/var/tmp/tmc-github-auth'}
if (r/'release-naming-verification.json').exists():
 raise SystemExit('Release already published and renamed to v10. See release-naming-verification.json; do not republish.')
def run(*args):return subprocess.check_output(list(map(str,args)),cwd=home,env=env,text=True)
def api(p,*args):return json.loads(run(gh,'api',p,*args))
def releases():return [x for p in json.loads(run(gh,'api','--paginate','--slurp','repos/'+repo+'/releases?per_page=100')) for x in p]
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
a=json.loads((r/'artifact.json').read_text());q=json.loads((r/'final-qa.json').read_text());assert q['passed'] and q['sha256']==a['sha256'] and q['kernel_regressions']=={'passed':29,'total':29}
iso=Path('/var/tmp/tmc10-final')/a['filename'];assert sha(iso)==a['sha256']
for p,h in json.loads((r/'source-hashes.json').read_text()).items():assert sha(home/'tinymacOS'/p)==h,p
assert api('repos/'+repo)['private']
git=['git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git','--work-tree='+str(home)];source=run(*git,'rev-parse','HEAD').strip();assert api('repos/'+repo+'/commits/main')['sha']==source
assert api('repos/'+repo+'/git/ref/tags/'+tag)['object']['sha']==source
before=releases();matches=[x for x in before if x['tag_name']==tag];assert len(matches)==1
j=matches[0];assert j['draft'] and not j['prerelease'];assets={x['name']:x for x in j['assets']}
assert set(assets)=={iso.name,'SHA256SUMS.txt'} and assets[iso.name]['digest']=='sha256:'+a['sha256'] and assets[iso.name]['size']==a['bytes']
w=Path(tempfile.mkdtemp(prefix='tmc10-remote-verified-',dir='/var/tmp'));draft=w/'draft';draft.mkdir()
run(gh,'release','download',tag,'--repo',repo,'--dir',draft)
assert sha(draft/iso.name)==a['sha256'] and (draft/'SHA256SUMS.txt').read_bytes()==(r/'SHA256SUMS.txt').read_bytes()
new=api('repos/'+repo+'/releases/'+str(j['id']),'--method','PATCH','-f','tag_name='+tag,'-F','draft=false','-F','prerelease=false','-f','make_latest=true');assert not new['draft'] and not new['prerelease'] and new['tag_name']==tag
published=w/'published';published.mkdir();run(gh,'release','download',tag,'--repo',repo,'--dir',published)
assert sha(published/iso.name)==a['sha256'] and (published/'SHA256SUMS.txt').read_bytes()==(r/'SHA256SUMS.txt').read_bytes()
changed=[]
for old in before:
 if old['id']==new['id']:continue
 if not old['prerelease']:
  updated=api('repos/'+repo+'/releases/'+str(old['id']),'--method','PATCH','-F','prerelease=true');assert updated['prerelease']
 changed.append({'id':old['id'],'tag':old['tag_name'],'was_prerelease':old['prerelease'],'prerelease':True})
all_releases=releases();assert all(x['prerelease'] for x in all_releases if x['id']!=new['id']);assert api('repos/'+repo+'/releases/latest')['id']==new['id'];final=api('repos/'+repo+'/releases/tags/'+tag);assert final['id']==new['id'] and not final['draft'] and not final['prerelease'];assert api('repos/'+repo+'/git/ref/tags/'+tag)['object']['sha']==source
record={'published':True,'version':'10.0','build':'30A1','release_id':new['id'],'url':final['html_url'],'tag':tag,'tag_commit':source,'normal_release':True,'all_older_releases_prerelease':True,'older_releases':changed,'iso_sha256':a['sha256'],'bytes':a['bytes'],'remote_download_verified':True,'draft_download_verified':True,'published_final_name_download_verified':True,'remote_download_external':str(published),'assets':[{'id':x['id'],'name':x['name'],'size':x['size'],'url':x['browser_download_url']} for x in final['assets']]}
(r/'github-release-verification.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record,indent=2))
