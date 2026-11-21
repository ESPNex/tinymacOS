#!/usr/bin/env python3
"""User-authorized naming-only change: v10.0 -> v10 and clean ISO names.
Keep ISO asset IDs/content/digests; replace checksum text transactionally.
Existing release IDs and normal/prerelease states must remain identical.
"""
import hashlib,json,os,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parent;repo='ESPNex/tinymacOS';gh='/var/tmp/tmc-github-cli/gh';env={**os.environ,'GH_CONFIG_DIR':'/var/tmp/tmc-github-auth'}
def run(*args):return subprocess.check_output(list(map(str,args)),env=env,text=True)
def api(p,*args):return json.loads(run(gh,'api',p,*args))
def releases():return [x for page in json.loads(run(gh,'api','--paginate','--slurp','repos/'+repo+'/releases?per_page=100')) for x in page]
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=releases();current=next(x for x in before if x['tag_name']=='v10.0');assert not any(x['tag_name']=='v10' for x in before)
ref=api('repos/'+repo+'/git/ref/tags/v10.0');assert ref['object']['type']=='commit';source=ref['object']['sha']
assert not any(x['ref']=='refs/tags/v10' for x in api('repos/'+repo+'/git/matching-refs/tags/v10'))
work=Path(tempfile.mkdtemp(prefix='tmc-release-naming-',dir='/var/tmp'));(work/'before.json').write_text(json.dumps(before,indent=2))
api('repos/'+repo+'/git/refs','--method','POST','-f','ref=refs/tags/v10','-f','sha='+source)
record={'passed':False,'naming_only':True,'source_commit_unchanged':source,'work_external':str(work),'releases':[]}
for old in before:
 tag='v10' if old['id']==current['id'] else old['tag_name'];isos=[x for x in old['assets'] if x['name'].endswith('.iso')];assert len(isos)==1
 asset=isos[0];old_name=asset['name'];new_name=old_name.replace('-OpenCore-Installer','').replace('-OpenCore','')
 if old['id']==current['id']:new_name=new_name.replace('-10.0-','-10-')
 body=old['body'] or '';body=body.replace(old_name,new_name)
 if old['id']==current['id']:body=body.replace('v10.0','v10').replace('10.0 / 30A1','10 / 30A1')
 changes=['--method','PATCH','-f','body='+body]
 if old['id']==current['id']:changes+=['-f','tag_name=v10','-f','name='+old['name'].replace('10.0','10'),'-f','make_latest=true']
 j=api('repos/'+repo+'/releases/'+str(old['id']),*changes);assert j['id']==old['id'] and j['prerelease']==old['prerelease'] and j['draft']==old['draft']
 item={'release_id':old['id'],'old_tag':old['tag_name'],'tag':tag,'iso_asset_id':asset['id'],'old_filename':old_name,'filename':new_name,'sha256':asset['digest'].removeprefix('sha256:'),'bytes':asset['size'],'prerelease':old['prerelease']}
 w=work/tag;w.mkdir()
 run(gh,'release','download',tag,'--repo',repo,'--pattern','SHA256SUMS.txt','--dir',w)
 expected=item['sha256']+'  '+new_name+'\n';oldsum=next(x for x in old['assets'] if x['name']=='SHA256SUMS.txt')
 assert (w/'SHA256SUMS.txt').read_text().strip()==item['sha256']+'  '+old_name
 if new_name!=old_name:
  updated=api('repos/'+repo+'/releases/assets/'+str(asset['id']),'--method','PATCH','-f','name='+new_name);assert updated['digest']==asset['digest'] and updated['size']==asset['size']
  candidate=w/'SHA256SUMS-renamed.txt';candidate.write_text(expected)
  run(gh,'release','upload',tag,candidate,'--repo',repo)
  state=api('repos/'+repo+'/releases/'+str(old['id']));candidate_asset=next(x for x in state['assets'] if x['name']==candidate.name)
  api('repos/'+repo+'/releases/assets/'+str(oldsum['id']),'--method','PATCH','-f','name=SHA256SUMS-before-rename.txt')
  api('repos/'+repo+'/releases/assets/'+str(candidate_asset['id']),'--method','PATCH','-f','name=SHA256SUMS.txt')
 verified=w/'verified';verified.mkdir();run(gh,'release','download',tag,'--repo',repo,'--pattern',new_name,'--pattern','SHA256SUMS.txt','--dir',verified)
 assert sha(verified/new_name)==item['sha256'] and (verified/new_name).stat().st_size==item['bytes'];assert (verified/'SHA256SUMS.txt').read_text()==expected
 if new_name!=old_name:run(gh,'api','--method','DELETE','repos/'+repo+'/releases/assets/'+str(oldsum['id']))
 item['remote_download_sha256_verified']=True;item['checksum_filename_verified']=True;record['releases'].append(item)
 print('VERIFIED',tag,new_name,flush=True)
after=releases();assert {x['id'] for x in after}=={x['id'] for x in before}
for old in before:
 new=next(x for x in after if x['id']==old['id']);item=next(x for x in record['releases'] if x['release_id']==old['id']);assert new['tag_name']==item['tag'] and new['prerelease']==old['prerelease'] and new['draft']==old['draft'];assert {x['name'] for x in new['assets']}=={item['filename'],'SHA256SUMS.txt'}
assert api('repos/'+repo+'/releases/latest')['id']==current['id'];assert api('repos/'+repo+'/git/ref/tags/v10')['object']['sha']==source
assert api('repos/'+repo+'/git/ref/tags/v10.0')['object']['sha']==source
run(gh,'api','--method','DELETE','repos/'+repo+'/git/refs/tags/v10.0')
record.update(passed=True,current_url='https://github.com/'+repo+'/releases/tag/v10',old_v10_0_tag_removed=True,all_iso_bytes_unchanged=True,release_states_preserved=True)
(r/'release-naming-verification.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record,indent=2))
