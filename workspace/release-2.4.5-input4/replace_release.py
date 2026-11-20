#!/usr/bin/env python3
"""Explicitly authorized replacement of existing v2.4.5, via GitHub CLI.
Upload and verify candidate before renaming old assets. Preserve original tag
as v2.4.5-21B8 before replacing v2.4.5 with the verified new source commit.
Never force-push main, never publish probes, never infer a successful test.
"""
import hashlib,json,os,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parent;home=r.parents[1]
repo='ESPNex/tinymacOS';tag='v2.4.5';gh='/var/tmp/tmc-github-cli/gh'
env={**os.environ,'GH_CONFIG_DIR':'/var/tmp/tmc-github-auth'}
def run(*c):return subprocess.check_output(list(map(str,c)),cwd=home,env=env,text=True)
def api(p,*args):return json.loads(run(gh,'api',p,*args))
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def patch_asset(asset,name):return api('repos/'+repo+'/releases/assets/'+str(asset['id']),'--method','PATCH','-f','name='+name)
a=json.loads((r/'artifact.json').read_text());qa=json.loads((r/'final-qa.json').read_text())
assert qa['passed'] and qa['sha256']==a['sha256'] and qa['kernel_regressions']=={'passed':29,'total':29}
assert qa['mission_control_chrome_and_menu_alignment_passed']
assert qa['sole_italian_keyboard_passed'] and qa['italian_app_profiles_passed']==4 and qa['italian_recovery_profiles_passed']==4
assert qa['independent_x_y_profiles_passed']==27 and qa['vertical_only_decoder_negative_control_passed']
assert qa['forced_pointer_transport_verified'] and qa['usb_keyboard_profiles_without_i8042'] and qa['prior_21B8_features_regression_passed']
assert qa['live_input_profiles_passed']==17 and qa['installed_input_profiles_passed']==10 and qa['f11_diagnostic_absent']
iso=Path('/var/tmp/tmc245-input4-final')/a['filename'];assert sha(iso)==a['sha256'] and iso.stat().st_size==a['bytes']
assert (r/'SHA256SUMS.txt').read_text()==a['sha256']+'  '+iso.name+'\n'
assert api('repos/'+repo)['private']
git=['git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git','--work-tree='+str(home)]
source=run(*git,'rev-parse','HEAD').strip();assert api('repos/'+repo+'/commits/main')['sha']==source
assert not run(*git,'status','--porcelain').strip(),'Source and QA must be committed first'
old=api('repos/'+repo+'/releases/tags/'+tag);assert not old['draft']
assets={x['name']:x for x in old['assets']};old_iso=assets[iso.name];old_sum=assets['SHA256SUMS.txt']
assert old_iso['digest']=='sha256:'+a['prior_sha256'],'Unexpected current asset: refusing to overwrite another revision'
ref=api('repos/'+repo+'/git/ref/tags/'+tag);old_source=ref['object']['sha']
assert ref['object']['type']=='commit'
prior={'release_id':old['id'],'tag':tag,'tag_commit':old_source,'iso_sha256':a['prior_sha256'],'assets':[{'id':x['id'],'name':x['name'],'size':x['size']} for x in [old_iso,old_sum]]}
(r/'previous-release.json').write_text(json.dumps(prior,indent=2)+'\n')
w=Path(tempfile.mkdtemp(prefix='tmc245-input4-publish-',dir='/var/tmp'))
candidate=w/'tinymacOS-2.4.5-21B9-candidate.iso';candidate.symlink_to(iso)
checksum=w/'SHA256SUMS-21B9-candidate.txt';checksum.write_bytes((r/'SHA256SUMS.txt').read_bytes())
run(gh,'release','upload',tag,candidate,checksum,'--repo',repo)
download=w/'candidate-download';download.mkdir()
run(gh,'release','download',tag,'--repo',repo,'--pattern',candidate.name,'--pattern',checksum.name,'--dir',download)
assert sha(download/candidate.name)==a['sha256'] and (download/checksum.name).read_bytes()==checksum.read_bytes()
current=api('repos/'+repo+'/releases/tags/'+tag);current_assets={x['name']:x for x in current['assets']}
new_iso=current_assets[candidate.name];new_sum=current_assets[checksum.name]
assert new_iso['digest']=='sha256:'+a['sha256'] and new_iso['size']==a['bytes']
# Archive the old source tag first. The published release ID remains unchanged.
api('repos/'+repo+'/git/refs','--method','POST','-f','ref=refs/tags/v2.4.5-21B8','-f','sha='+old_source)
patch_asset(old_iso,'tinymacOS-2.4.5-21B8-superseded.iso')
patch_asset(old_sum,'SHA256SUMS-21B8-superseded.txt')
patch_asset(new_iso,iso.name);patch_asset(new_sum,'SHA256SUMS.txt')
# The user explicitly requested overwriting this release. Only this tag moves;
# old source remains addressable via its archived tag and the preserved receipt.
api('repos/'+repo+'/git/refs/tags/'+tag,'--method','PATCH','-f','sha='+source,'-F','force=true')
run(gh,'release','edit',tag,'--repo',repo,'--title','tinymacOS 2.4.5 Levante — Tastiera ITA + assi mouse (21B9)','--notes-file',r/'RELEASE.md','--latest')
final=w/'published-download';final.mkdir()
run(gh,'release','download',tag,'--repo',repo,'--pattern',iso.name,'--pattern','SHA256SUMS.txt','--dir',final)
assert sha(final/iso.name)==a['sha256'] and (final/'SHA256SUMS.txt').read_bytes()==checksum.read_bytes()
# Delete only the two superseded assets, after the replacements were downloaded
# and verified under their final names. IDs were read from the original release.
for asset in [old_iso,old_sum]:run(gh,'api','--method','DELETE','repos/'+repo+'/releases/assets/'+str(asset['id']))
j=api('repos/'+repo+'/releases/tags/'+tag);tag_ref=api('repos/'+repo+'/git/ref/tags/'+tag)
assert j['id']==old['id'] and not j['draft'] and tag_ref['object']['sha']==source
assert {x['name'] for x in j['assets']}=={iso.name,'SHA256SUMS.txt'}
record={'published':True,'replaced_existing_release':True,'release_id':j['id'],'url':j['html_url'],'tag':tag,'tag_commit':source,'archived_prior_tag':'v2.4.5-21B8','build':'21B9','iso_sha256':a['sha256'],'bytes':a['bytes'],'remote_download_verified':True,'downloaded_external':str(final),'assets':[{'id':x['id'],'name':x['name'],'size':x['size'],'url':x['browser_download_url']} for x in j['assets']]}
(r/'github-release-verification.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record,indent=2),flush=True)
