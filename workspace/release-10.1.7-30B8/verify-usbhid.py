#!/usr/bin/env python3
"""Read-only candidate verifier. No publication and no physical certification."""
from pathlib import Path
import gzip,hashlib,json,sys
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS'
def sha(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
def runtime_tree():
 rows={str(p.relative_to(root)):sha(p) for p in sorted(root.rglob('*')) if p.is_file() and '__pycache__' not in p.parts and p.suffix in ('.c','.h','.S','.inc','.ld','.sh','.py')}
 for delta in r.get('non_runtime_source_delta',[]):
  assert delta['path']=='tools/sourcekit.py' and rows[delta['path']]==delta['new_sha256']
  rows[delta['path']]=delta['old_sha256']
 return hashlib.sha256(json.dumps(rows,sort_keys=True,separators=(',',':')).encode()).hexdigest()
r=json.loads((q/'usbhid-validation.json').read_text());e=json.loads(gzip.decompress((q/r['evidence_file']).read_bytes()))
assert sha(q/r['evidence_file'])==r['evidence_sha256']
assert runtime_tree()==r['runtime_tree_sha256'],'Runtime source tree differs from tested candidate'
assert len(e['usb_guests'])==8
for g in e['usb_guests']:
 assert g['passed'] and g['usbhid_tested'] and g['iso_sha256']==r['iso']['sha256']
 assert g['kernelcache_sha256']==r['kernelcache']['sha256'] and g['worker_polls_after']>g['worker_polls_before']
assert len(e['kernel_guests'])==13 and len(e['kernel_media'])==5
for g in e['kernel_guests']:assert g['passed']
for m in e['kernel_media']:
 assert m['passed'] and m['actual_iso_kernel_equals_current_production'] and m['kernelcache_sha256']==r['kernelcache']['sha256']
for name,g in e['gates'].items():
 assert g['passed'],name
 if 'iso_sha256' in g:assert g['iso_sha256']==r['iso']['sha256'],name
media=Path(sys.argv[1]) if len(sys.argv)>1 else Path(r['iso']['external_path'])
if media.exists():assert sha(media)==r['iso']['sha256'] and media.stat().st_size==r['iso']['bytes']
print(json.dumps({'receipt_verified':True,'runtime_sources_match':True,'usb_guests':8,'kernel_guests':13,'actual_local_iso_verified':media.exists(),'candidate_software_gates_passed':True,'production_port_complete':False,'physical_hardware_tested':False},indent=2))
