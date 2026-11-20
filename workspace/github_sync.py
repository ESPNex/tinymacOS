#!/usr/bin/env python3
"""Back up the workspace to its private repo without storing Git/auth in snapshots.
Default is status only. Use --push -m '...' to commit and push. Never force-push.
If local Git metadata was lost, fetches the existing remote without checking out
or overwriting workspace files. Missing files block the commit unless deletion
was explicitly requested. A complete recovery should use a normal git clone.
"""
import argparse,os,re,shutil,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--push',action='store_true');ap.add_argument('-m','--message',default='Synchronize tinymacOS development workspace')
ap.add_argument('--allow-delete',action='store_true');args=ap.parse_args()
gitdir=Path(os.environ.get('TMC_GIT_DIR','/var/tmp/tmc-github-git/tinymacOS.git')).resolve()
if gitdir.is_relative_to(root):raise SystemExit('Git metadata must be outside the workspace to preserve snapshot budget.')
gh=os.environ.get('GH_BIN') or shutil.which('gh') or '/var/tmp/tmc-github-cli/gh'
if not Path(gh).is_file():raise SystemExit('Install official GitHub CLI and authorize gh auth login first; no tokens in this script.')
env=os.environ.copy()
if 'GH_CONFIG_DIR' not in env and Path('/var/tmp/tmc-github-auth').is_dir():env['GH_CONFIG_DIR']='/var/tmp/tmc-github-auth'
repo='ESPNex/tinymacOS';remote='https://github.com/'+repo+'.git'
base=['git','--git-dir='+str(gitdir),'--work-tree='+str(root)]
def run(cmd,**kw):return subprocess.run(cmd,cwd=root,env=env,check=True,**kw)
def output(cmd):return run(cmd,stdout=subprocess.PIPE).stdout
helper=['-c','credential.helper=','-c','credential.helper=!'+str(gh)+' auth git-credential']
run([gh,'api','user','--silent'])
if not (gitdir/'HEAD').exists():
 gitdir.parent.mkdir(parents=True,exist_ok=True)
 run(['git','init','--bare','--initial-branch=main',str(gitdir)])
 run(base+['config','core.bare','false']);run(base+['config','core.worktree',str(root)])
 run(base+['remote','add','origin',remote]);run(base+helper+['fetch','origin','main'])
 run(base+['update-ref','refs/heads/main','FETCH_HEAD']);run(base+['reset','--mixed','HEAD'])
actual=output(base+['remote','get-url','origin']).decode().strip()
if actual!=remote:raise SystemExit('Unexpected origin; refusing to publish to another repository.')
if not args.push:
 run(base+['status','--short']);raise SystemExit(0)
privacy=output([gh,'repo','view',repo,'--json','isPrivate','--jq','.isPrivate']).strip()
if privacy!=b'true':raise SystemExit('Repository is not private; refusing to publish.')
run(base+['add','--all'])
deleted=output(base+['diff','--cached','--name-only','--diff-filter=D','-z'])
if deleted and not args.allow_delete:raise SystemExit('Staged deletions found. Check missing snapshot files; restore from GitHub, or explicitly --allow-delete for intended deletions.')
patterns=[rb'gh[pousr]_[A-Za-z0-9]{30,}',rb'github_pat_[A-Za-z0-9_]{40,}',rb'AKIA[A-Z0-9]{16}',rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',rb'https://[^\s/@:]+:[^\s/@]+@github\.com']
changed=output(base+['diff','--cached','--name-only','--diff-filter=ACMR','-z']).decode().strip('\0').split('\0')
for name in filter(None,changed):
 data=output(base+['show',':'+name])
 if any(re.search(pattern,data) for pattern in patterns):raise SystemExit('Possible credential in staged file: '+name+'; review before publishing.')
 if len(data)>95*1024*1024:raise SystemExit('Oversize staged file: '+name)
run(base+['config','user.name','Arena workspace backup']);run(base+['config','user.email','arena-workspace@users.noreply.github.com'])
if output(base+['diff','--cached','--name-only']).strip():run(base+['commit','-m',args.message])
run(base+helper+['push','--set-upstream','origin','main'])
local=output(base+['rev-parse','HEAD']).decode().strip()
remote_sha=output([gh,'api','repos/'+repo+'/commits/main','--jq','.sha']).decode().strip()
if local!=remote_sha:raise SystemExit('Remote verification mismatch; do not claim a completed backup.')
print('Verified private backup: https://github.com/'+repo+'/commit/'+local)
