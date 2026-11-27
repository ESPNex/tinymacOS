"""M6 wording/units guard. Static provenance is NOT graphical acceptance."""
from pathlib import Path
import hashlib,json,re,runpy
from decimal import Decimal
R=Path(__file__).resolve().parents[1]
m=runpy.run_path(str(R/'tools/audit-ui-strings.py'));lex=m['literals'];token=m['TOKEN']
policy=json.loads((R/'tests/fixtures/localization6-contract.json').read_text())
def canonical(s):return re.sub(r'\s+','',token.sub(lambda m:'"TEXT"' if m.group('string') else "'CHAR'" if m.group('char') else '',s))
for f,h in policy['algorithm_and_abi_outside_literals_sha256'].items():
 assert hashlib.sha256(canonical((R/f).read_text()).encode()).hexdigest()==h,(f,'non-wording behavior/ABI change')

def no_binary_ui(s):
 for _,_,text,ctx in lex(s):
  assert not re.search(r'\b[KMGTPE]iB\b',text),('binary size label in production literal',text)
files=[p for d in ('userspace','desktop','include','boot','fs','storage') for p in (R/d).glob('*') if p.suffix in ('.c','.h')]
for p in files:no_binary_ui(p.read_text())
no_binary_ui('/* 16 TiB is a valid internal geometry comment */ const char*s="1.0 TB";')
for invalid in ('MiB','GiB','TiB','KiB'):
 try:no_binary_ui('const char*s="1 '+invalid+'";')
 except AssertionError:pass
 else:raise AssertionError('binary-label mutation was not rejected')
# Exact SI equivalents, not a dangerous rename of a binary threshold.
for f,key in [('storage/partition.c','capacity_bytes'),('storage/diskedit.c','resize_bytes')]:
 exact=str(Decimal(policy['binary_limits'][key])/Decimal(10**12))+' TB'
 assert exact in (R/f).read_text(),(f,exact)
required={
 'desktop/login.c':['Crea un account computer','Nome account','Account temporaneo finché non installi tinymacOS.'],
 'desktop/recovery.c':['Ripara o inizializza un disco.','I dati verranno eliminati.','Confermi l’operazione sul disco selezionato?','copia locale'],
 'userspace/settings.c':['Schermi','Privacy e sicurezza','Riduci trasparenza','Riduci movimento','Nessun backup disponibile.'],
 'userspace/monitor.c':['Memoria utilizzata','Cache inclusa, non conteggiata separatamente.','CPU normalizzata sul totale dei core.'],
 'userspace/filepanel.c':['Salva col nome:','La destinazione non è scrivibile.','Il file esistente verrà sostituito.'],
}
for f,ss in required.items():
 for s in ss:assert s in (R/f).read_text(),(f,s)
for f in ('userspace/preview.c','userspace/textedit.c','userspace/finder.c'):
 assert 'display_size(' in (R/f).read_text(),f
assert 'Ripristina da Time Machine' not in (R/'desktop/recovery.c').read_text(),'Do not imply external Time Machine compatibility for same-disk snapshots'
print('PASS M6 static wording/units: all '+str(len(files))+' native source files scanned, binary-label mutations rejected; exact SI backend limits; 8 algorithm/ABI identities and truthful safety/capability terms retained')
