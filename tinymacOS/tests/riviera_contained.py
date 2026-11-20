"""An ISO → installed GPT byte audit and cold-boot proof using only the installed disk.
Input is the lab fixture produced by riviera_live. Never change it; only modify a fresh clone.
"""
import hashlib,json,os,shutil,subprocess,sys,time
from pathlib import Path
from retail_guest import Retail
from boot_recovery import RecoveryKey
from riviera_media import gpt,H

iso=Path(sys.argv[1]).resolve();src=Path(sys.argv[2]).resolve();out=Path(sys.argv[3]).resolve();assert not out.is_relative_to('/home/user');out.mkdir(parents=True,exist_ok=False);e=out/'evidence';e.mkdir()
tool=os.environ.get('TOOLROOT','');env={**os.environ,'PATH':tool+'/usr/bin:'+os.environ['PATH'],'LD_LIBRARY_PATH':tool+'/usr/lib/x86_64-linux-gnu:'+os.environ.get('LD_LIBRARY_PATH','')}
def run(*cmd):return subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True).stdout
base=out/'from-iso.dmg';on_disk=out/'from-disk.dmg';part=out/'part-3.fat';snap=gpt(src);part.write_bytes(snap['rec']);start=snap['parts'][2][0];del snap
run('xorriso','-osirrox','on','-indev',iso,'-extract','/Recovery/BaseSystem.dmg',base);run('mcopy','-i',part,'::/Recovery/BaseSystem.dmg',on_disk)
assert base.read_bytes()==on_disk.read_bytes();raw=base.read_bytes();assert raw[-512:-508]==b'koly'
files=run('mdir','-i',part,'-/','-b','::/').decode().splitlines();assert any(f.lower().endswith('/recovery/basesystem.dmg') for f in files)
record={'iso':iso.name,'iso_sha256':H(iso.read_bytes()),'disk_partition':3,'first_sector':start,'files':files,'dmg_in_iso':H(raw),'dmg_on_disk':H(on_disk.read_bytes()),'dmg_bytes':len(raw),'checks':[]}
def ok(s):record['checks'].append(s);(e/'contained.json').write_text(json.dumps(record,indent=2,ensure_ascii=False));print('OK:',s,flush=True)
ok('Due estrazioni indipendenti: ISO con xorriso e partizione GPT 3 del disco installato con mcopy. /Recovery/BaseSystem.dmg identico byte per byte, non un collegamento o un installer esterno')
class DiskRecovery(RecoveryKey,Retail):pass
disk=out/'local.raw';shutil.copyfile(src,disk);os.environ['DISK']=str(disk)
def media(g):
    units=[b['inserted']['file'] for b in g.req('query-block') if 'inserted' in b];u=g.hmp('info usb');assert str(disk) in units and all(str(iso)!=name and not name.endswith(('.iso','.img','.dmg')) for name in units) and 'USB MSD' not in u,(units,u)
    assert g.req('query-mice')[0]['absolute'];record['disk_only_media']=[Path(n).name for n in units];record['usb']=u.strip().splitlines()
with DiskRecovery('/not-attached',out,e,'local',disk='cold-nvram',boot_disk=True) as g:
    media(g)
    for word in ('Terminale','Continua'):
        x,y=g.visible(word);g.click(x+7,y+6)
    g.visible('Shell di recupero locale');g.cmd('sw_vers','tinymacOS Recovery 2.4.4');g.cmd('diskutil info /','Origine: disco interno, partizione Recovery');g.visible(str(len(raw))+' byte verificati');g.shot('origine-disco')
    ok('NVRAM nuova; solo il disco SATA installato, tastiera e tablet. R alla mela apre il DMG locale. Due clic aprono la shell: tipo, partizione e misura derivano dall’effettivo volume UEFI sorgente')
# Change only a byte in XML whitespace (not a FAT sector and not an executable).
# The desktop loader must reject the damaged disk image, not run another copy.
outer=part.read_bytes();off=outer.index(raw);assert outer.count(raw)==1;where=start*512+off+len(raw)-513
with disk.open('r+b') as f:f.seek(where);old=f.read(1);assert old==b'\n';f.seek(where);f.write(b' ')
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(disk)
class Damaged(RecoveryKey,Retail):recovery_text=None
with Damaged('/not-attached',out,e,'alterato',disk='isolated-nvram',boot_disk=True) as g:
    media(g);end=time.monotonic()+24
    while time.monotonic()<end:
        t=g.ocr(psm=6);lower=''.join(c for c in t.casefold() if c.isalnum());
        if 'recuperonondisponibile' in lower and 'recuperononeseguito' in lower:break
        time.sleep(.2)
    else:raise AssertionError('Messaggio UEFI di rifiuto non trovato: '+g.ocr())
    g.snap().save(e/'immagine-rifiutata.png')
assert sha(disk)==before;ok('Su un clone, cambiato un solo spazio nel metadato XML del DMG installato: il loader ha rifiutato il Recupero. Nessun secondo runtime usato. SHA-256 del disco invariato dopo il tentativo')
