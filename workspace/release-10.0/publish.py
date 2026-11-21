#!/usr/bin/env python3
"""Publish verified 30A1 through authorized GitHub CLI; no force/deletions.
Source/QA must already be pushed. New release is normal, all older releases
become prereleases only AFTER a final-name remote asset download verifies.
"""
import hashlib,json,os,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parent;home=r.parents[1];repo='ESPNex/tinymacOS';tag='v10.0';gh='/var/tmp/tmc-github-cli/gh'
env={**os.environ,'GH_CONFIG_DIR':'/var/tmp/tmc-github-auth'}
def run(*args):return subprocess.check_output(list(map(str,args)),cwd=home,env=env,text=True)
def api(p,*args):return json.loads(run(gh,'api',p,*args))
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
a=json.loads((r/'artifact.json').read_text());q=json.loads((r/'final-qa.json').read_text());assert q['passed'] and q['sha256']==a['sha256'] and q['version']=='10.0' and q['build']=='30A1'
assert q['kernel_regressions']=={'passed':29,'total':29} and q['independent_x_y_profiles_passed']==27
assert q['italian_app_profiles_passed']==q['italian_recovery_profiles_passed']==4
for flag in ['independent_clean_rebuild_identical','native_kernel_matches_production','mission_control_chrome_and_menu_alignment_passed','icon_atlas_and_filesystem_verified','latest_exact_name_variants_verified','split_semibold_wordmark_verified','recovery_alignment_and_smaller_title_verified']:assert q[flag],flag
iso=Path('/var/tmp/tmc10-final')/a['filename'];assert sha(iso)==a['sha256'] and iso.stat().st_size==a['bytes'];assert (r/'SHA256SUMS.txt').read_text()==a['sha256']+'  '+iso.name+'\n'
assert api('repos/'+repo)['private']
git=['git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git','--work-tree='+str(home)]
source=run(*git,'rev-parse','HEAD').strip();assert api('repos/'+repo+'/commits/main')['sha']==source
assert not run(*git,'status','--porcelain').strip(),'Commit and push all source and QA first'
# List all releases using pagination before touching release state.
pages=json.loads(run(gh,'api','--paginate','--slurp','repos/'+repo+'/releases?per_page=100'));before=[x for page in pages for x in page]
assert all(x['tag_name']!=tag for x in before),'v10.0 already exists: inspect its receipt, do not overwrite'
# Check tag collisions; only a new tag or an identical current-source tag is safe.
refs=api('repos/'+repo+'/git/matching-refs/tags/v10.0');matches=[x for x in refs if x['ref']=='refs/tags/'+tag]
if matches:assert len(matches)==1 and matches[0]['object']['type']=='commit' and matches[0]['object']['sha']==source
else:api('repos/'+repo+'/git/refs','--method','POST','-f','ref=refs/tags/'+tag,'-f','sha='+source)
run(gh,'release','create',tag,'--repo',repo,'--verify-tag','--draft','--title','tinymacOS 10 Tramontana — 30A1','--notes-file',r/'RELEASE.md')
run(gh,'release','upload',tag,iso,r/'SHA256SUMS.txt','--repo',repo)
# Draft releases can return 404 through the by-tag REST endpoint.
created=json.loads(run(gh,'api','--paginate','--slurp','repos/'+repo+'/releases?per_page=100'))
j=next(x for page in created for x in page if x['tag_name']==tag);assert j['draft'] and not j['prerelease'];assets={x['name']:x for x in j['assets']}
assert set(assets)=={iso.name,'SHA256SUMS.txt'} and assets[iso.name]['digest']=='sha256:'+a['sha256'] and assets[iso.name]['size']==a['bytes']
w=Path(tempfile.mkdtemp(prefix='tmc10-remote-release-',dir='/var/tmp'))
run(gh,'release','download',tag,'--repo',repo,'--dir',w)
assert sha(w/iso.name)==a['sha256'] and (w/'SHA256SUMS.txt').read_bytes()==(r/'SHA256SUMS.txt').read_bytes()
run(gh,'release','edit',tag,'--repo',repo,'--draft=false','--prerelease=false','--latest')
new=api('repos/'+repo+'/releases/tags/'+tag);assert not new['draft'] and not new['prerelease']
changed=[]
for old in before:
 if old['id']==new['id']:continue
 if not old['prerelease']:
  updated=api('repos/'+repo+'/releases/'+str(old['id']),'--method','PATCH','-F','prerelease=true');assert updated['prerelease']
 changed.append({'id':old['id'],'tag':old['tag_name'],'was_prerelease':old['prerelease'],'prerelease':True})
pages=json.loads(run(gh,'api','--paginate','--slurp','repos/'+repo+'/releases?per_page=100'));all_releases=[x for p in pages for x in p]
assert all(x['prerelease'] for x in all_releases if x['id']!=new['id'])
ref=api('repos/'+repo+'/git/ref/tags/'+tag);assert ref['object']['sha']==source
assert api('repos/'+repo+'/releases/latest')['id']==new['id']
record={'published':True,'version':'10.0','build':'30A1','release_id':new['id'],'url':new['html_url'],'tag':tag,'tag_commit':source,'normal_release':True,'all_older_releases_prerelease':True,'older_releases':changed,'iso_sha256':a['sha256'],'bytes':a['bytes'],'remote_download_verified':True,'remote_download_external':str(w),'assets':[{'id':x['id'],'name':x['name'],'size':x['size'],'url':x['browser_download_url']} for x in new['assets']]}
(r/'github-release-verification.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record,indent=2))
