"""Simulate media bit errors on a disposable QA clone, not on an original or user disk."""
import hashlib,json,os,shutil,sys
from pathlib import Path
from riviera_media import gpt
from retail_guest import Retail
from boot_recovery import RecoveryKey
src=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]).resolve();out.mkdir(parents=True,exist_ok=False);ev=out/'evidence';ev.mkdir();target=out/'unreadable-copies.raw'
state=gpt(src);assert state['seal'][:8]==b'TMCSNAP2' and hashlib.sha256(state['snapshot']).digest()==state['seal'][32:64];a=state['parts'][3][0]; del state
shutil.copyfile(src,target)
with target.open('r+b') as f:
    # The metadata of both backups and the entire active system/data remain intact.
    for o in ((51200+57344+40)*512+3,(a+1+800)*512+11):
        f.seek(o);v=f.read(1);f.seek(o);f.write(bytes([v[0]^32]))
def digest(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=digest(target);os.environ['DISK']=str(target)
class Rec(RecoveryKey,Retail):initial='Scegli come proseguire.'
with Rec('/not-attached',out,ev,'damaged-sources',disk='nvram',boot_disk=True) as g:
    for i in range(2):
        if i:g.key('down',.22)
        g.key('ret');g.visible('Seleziona un disco');g.key('ret');g.visible('Conferma l’operazione');g.key('ret');g.visible('Operazione non riuscita.',45);g.shot('rifiuto-'+('reinstallazione' if i else 'ripristino'));g.key('ret');g.visible('Scegli come proseguire.')
after=digest(target);assert before==after
msg='Copie dati e sistema con alterazioni indipendenti: entrambi i ripristini rifiutati prima della prima scrittura; SHA-256 del disco intero invariato: '+after
(ev/'integrity.json').write_text(json.dumps([msg],ensure_ascii=False,indent=2));print('OK:',msg)
