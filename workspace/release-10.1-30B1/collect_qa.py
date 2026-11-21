#!/usr/bin/env python3
"""Final gate: every result must be fresh and bound to the release ISO/source."""
import hashlib,json,subprocess
from pathlib import Path
q=Path(__file__).parent;root=q.parents[1]/'tinymacOS';iso=Path('/var/tmp/tmc-10.1/tinymacOS-10.1-x86_64.iso')
def sha(p):return hashlib.file_digest(p.open('rb'),'sha256').hexdigest()
def get(n):return json.loads((q/n).read_text())
a=get('artifact.json');assert sha(iso)==a['sha256'] and iso.stat().st_size==a['bytes'];assert iso.read_bytes()==Path('/var/tmp/tmc-10.1/staged-repro.iso').read_bytes()
sources=get('source-hashes.json')
for p,h in sources.items():assert sha(root/p)==h,p
host=get('control-contract-result.json');assert host['passed']
for p,h in host['source_sha256'].items():assert sha(root/p)==h,p
names=['control-result.json','identity-result.json','bios-installer-result.json']+[f'input-{fw}-{p}.json' for fw in ['uefi','bios'] for p in ['ps2','ehci','xhci-hub-kbd']]+[f'recovery-keyboard-{fw}-ps2.json' for fw in ['uefi','bios']]
results={}
for n in names:
 j=get(n);assert j['passed'] and j['iso_sha256']==a['sha256'],n;results[n]=sha(q/n)
rec=get('bios-recovery-result.json');assert rec['passed'] and all(rec[k] for k in ['local_recovery_without_iso','sealed_esp_restored_byte_identical','documents_unchanged','duet_gap_unchanged','desktop_reboot_after_restore']);results['bios-recovery-result.json']=sha(q/'bios-recovery-result.json')
k=get('kernel-regression-result.json');assert k['passed'] and len(k['cases'])==29 and all(c['exit_code']==0 for c in k['cases'])
for n,h in k['production_match'].items():assert sha(Path('/var/tmp/tmc-10.1/build')/n)==h
results['kernel-regression-result.json']=sha(q/'kernel-regression-result.json');results['control-contract-result.json']=sha(q/'control-contract-result.json')
r={**a,'passed':True,'source_files_checked':len(sources),'black_box_control_checks':len(get('control-result.json')['checks']),'host_geometry_resolutions':5,'sanitizers':host['sanitizers'],'live_input_profiles':6,'recovery_italian_profiles':2,'kernel_regressions':29,'actual_bios_install_hdd_reboot_uefi_crossboot':True,'sealed_offline_recovery_reinstall':True,'physical_hardware_tested':False,'browser_included':False,'github_published':False,'source_repository_synced':False,'publication_blocker':'GitHub CLI authorization absent in current session','result_files_sha256':results}
(q/'final-qa.json').write_text(json.dumps(r,indent=2)+'\n')
summary=f'''# tinymacOS 10.1 Tramontana · 30B1

## Centro di Controllo
- Composizione modulare ispirata al riferimento: pillole, pulsanti circolari, riquadro con anteprima dello sfondo, due cursori orizzontali e scorciatoie.
- Nessun pannello pieno circostante. Stesso materiale **MAT_DOCK**, con vera convoluzione dello sfondo e rifrazione, non un'immagine sfocata sostitutiva.
- Sei preferenze reali: aspetto chiaro/scuro, tema, sfondo, Dock, movimento e trasparenza. I cursori selezionano gli **8 temi** e i **13 sfondi** esistenti: nessuna luminosità, radio o volume fittizi.
- Simboli di sistema interamente procedurali. Mission Control, blocco sessione, Calcolatrice, Calendario, Terminale e Impostazioni aprono le funzioni/app vere.
- Mouse: clic e trascinamento; tastiera: Tab/Maiusc+Tab, Invio/Spazio, frecce, Home/End, Esc. Geometria condivisa tra disegno e hit test, fallback opaco leggibile.

## Collaudi sulla ISO definitiva
- **{r['black_box_control_checks']} verifiche** black-box del pannello: etichette nel framebuffer, sfondo libero fra moduli, preferenze, trascinamento/rilascio, frecce, scorciatoie, blocco e sblocco reale.
- Contratti del codice di produzione con **AddressSanitizer e UndefinedBehaviorSanitizer**, geometria a 5 risoluzioni, bersagli disgiunti, gestione fallimenti syscall.
- **29/29 regressioni kernel/runtime**, con KernelCache e kernel.elf identici a quelli di produzione. Un timeout causato dalla pressione di memoria delle VM concorrenti è conservato nel verbale insieme alla ripetizione isolata riuscita; non è stato trasformato in PASS senza riesecuzione.
- **6 profili input reali QEMU**: BIOS/UEFI × PS/2, EHCI e xHCI con tastiera USB dietro hub.
- Installazione GUI BIOS, rimozione ISO, riavvio da disco, persistenza documento e avvio UEFI dello stesso disco.
- KernelCache corrotto su copia sacrificabile: rifiuto, Recovery locale senza ISO, reinstallazione sigillata, documenti preservati e ritorno alla scrivania.
- Tastiera italiana nel Recovery: **2 profili BIOS/UEFI**, versione 10.1/30B1 e caratteri accentati/AltGr verificati.
- Identità verificata nei plist di sistema e app, Terminale, About, installer e Recovery; sigilli/payload controllati da lettori indipendenti.
- **Due build indipendenti byte per byte identiche**. {len(sources)} file sorgente congelati e ricontrollati.

## Artefatto
`{a['filename']}` · {a['bytes']:,} byte

SHA-256:
```
{a['sha256']}
```

## Limiti e pubblicazione
I test sono virtuali, non certificano il mouse sul computer ASRock fisico. Safari/WebKit non incluso.

ISO generata e collaudata localmente. **Non ancora sincronizzata né pubblicata su GitHub**: l'autenticazione GitHub CLI della sessione precedente non è disponibile. La release v10 esistente non è stata modificata. Il download temporaneo viene fornito dalla sessione corrente.
'''
(q/'QA-SUMMARY.md').write_text(summary)
print('PASS FINAL GATE',a['sha256'],flush=True)
