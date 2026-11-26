"""Fail closed on stale QA media: bind image, kernel, source and actual ISO payload."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
def digest(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
def verify_probe_media(image,kind):
 results=Path(os.environ.get('TMC_QA_RESULTS',str(ROOT/'test-results')))
 image=Path(image);manifest=json.loads((results/('guest-'+kind+'-build.json')).read_text())
 assert image.name==manifest['iso_filename']=='tinymacOS-qa-probe-x86_64.iso'
 assert digest(image)==manifest['sha256'][image.name]
 assert digest(image.parent/'command.elf')==manifest['sha256']['command.elf'],'Stale diagnostic executable'
 probe_source=json.loads((results/('guest-'+kind+'-source-hashes.json')).read_text())
 for name,expected in probe_source.items():assert digest(name)==expected,'Probe source changed after compilation: '+name
 production=Path(os.environ.get('TMC_PRODUCTION_BUILD','/var/tmp/tmc-webkit-vm-system'))
 assert str(production)==manifest['production_build'] and manifest['kernel_equals_current_production']
 for name in ['kernel.elf','KernelCache']:
  assert digest(image.parent/name)==manifest['sha256'][name]==digest(production/name),name+' differs from current production'
 # Independent read of the kernel actually packed in the optical namespace.
 toolroot=Path(os.environ.get('TOOLROOT','/var/tmp/tmc/tools/root'))
 env={**os.environ,'LD_LIBRARY_PATH':str(toolroot/'usr/lib/x86_64-linux-gnu')}
 with tempfile.TemporaryDirectory(prefix='tmc-probe-payload-',dir='/var/tmp') as d:
  payload=Path(d)/'KernelCache'
  subprocess.run([str(toolroot/'usr/bin/xorriso'),'-osirrox','on','-indev',str(image),'-extract','/System/Library/KernelCollections/KernelCache',str(payload)],env=env,check=True,capture_output=True)
  assert digest(payload)==digest(production/'KernelCache'),'Actual QA ISO contains a stale kernel'
 # The fresh BASE build records these before compilation; verify all remain current.
 source=json.loads((results/'vm-system-source-hashes.json').read_text())
 osroot=ROOT.parent.parent/'tinymacOS'
 for name,expected in source.items():assert digest(osroot/name)==expected,'Source changed after fresh QA kernel build: '+name
 (results/('guest-'+kind+'-media.json')).write_text(json.dumps({'passed':True,'image':str(image),'iso_sha256':digest(image),'production_build':str(production),'kernel_sha256':digest(production/'kernel.elf'),'kernelcache_sha256':digest(production/'KernelCache'),'actual_iso_kernel_equals_current_production':True,'source_files_verified':len(source),'source_manifest_sha256':digest(results/'vm-system-source-hashes.json')},indent=2)+'\n')
