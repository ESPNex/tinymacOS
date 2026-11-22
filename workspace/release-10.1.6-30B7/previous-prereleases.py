#!/usr/bin/env python3
"""Standing user instruction: keep every preceding release as prerelease, preserve assets/tags/notes."""
import json,os,subprocess
from pathlib import Path
q=Path(__file__).resolve().parent;repo='ESPNex/tinymacOS';gh='/var/tmp/tmc-github-cli/gh';env={**os.environ,'GH_CONFIG_DIR':'/var/tmp/tmc-github-auth','GODEBUG':'http2client=0'}
def api(p,data=None):
 cmd=[gh,'api','repos/'+repo+'/'+p]
 if data is not None:cmd+=['--method','PATCH','--input','-']
 return json.loads(subprocess.check_output(cmd,input=json.dumps(data).encode() if data is not None else None,env=env))
before=api('releases?per_page=100');tags=api('tags?per_page=100');changed=[]
for r in before:
 if not r['draft'] and r['tag_name']!='v10.1.6' and not r['prerelease']:
  api('releases/'+str(r['id']),{'prerelease':True});changed.append(r['tag_name'])
after=api('releases?per_page=100');assert tags==api('tags?per_page=100');cases=[]
for old in before:
 if old['draft'] or old['tag_name']=='v10.1.6':continue
 new=next(n for n in after if n['id']==old['id']);assert new['prerelease'] and old['body']==new['body'] and old['target_commitish']==new['target_commitish']
 def assets(r):return sorted((a['id'],a['name'],a['size'],a.get('digest')) for a in r['assets'])
 assert assets(old)==assets(new)
 cases.append({'tag':old['tag_name'],'id':old['id'],'prerelease':True,'assets_unchanged':True,'notes_unchanged':True,'tag_target_unchanged':True})
assert len(cases)>=10
(q/'prerelease-conversion.json').write_text(json.dumps({'passed':True,'all_previous_releases_prerelease':True,'changed':changed,'cases':cases,'all_tag_objects_unchanged':True},indent=2)+'\n');print('PASS all previous releases prerelease, preserved assets/notes/tags:',len(cases),'changed:',changed)
