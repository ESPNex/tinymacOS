# tinymacOS 10 / 30A3 — QA finale

Gate `collect_qa.py`: **PASS**, tutti i risultati legati alla ISO di produzione.

- SHA-256: `2e4a2c84d66a87d081483da9f22b351b43dce0ff563fe0f5e5d8410fee96cc7b`.
- ISO: `tinymacOS-10-x86_64.iso`, 225599488 byte. Due build indipendenti byte-identiche; 517 input congelati verificati.
- Sorgente sincronizzato **prima** dell'intervento: commit `f604ba63a98e3424c56b5c8069a7443239308b62`, ricevuta `source-sync-before-fix.log`.
- **8/8** Recovery ottico BIOS: R/F12 × tastiera EHCI/xHCI × mouse/tablet. Menu osservato, clic Terminale/Continua, `sw_vers` e `diskutil info /` effettivamente eseguiti. Solo questa ISO collegata, nessun disco installato o Recovery esterno.
- Controllo negativo con la ISO pubblicata 30A2: mancata selezione riprodotta con lo **stesso harness finale**. Recovery ottico UEFI con entrambi i puntatori: PASS.
- Firmware estratto dalla ISO: tutti i 29 FFS originali conservati e 3 moduli upstream USB immutati; derivazione deterministica; nessuna strumentazione diagnostica distribuita. Firmware del disco installato invariato.
- Contratti host ASAN/UBSAN: PASS, inclusi firmware corrotto, conservazione moduli, report tastiera periodici senza duplicati, rilascio/ripetizione, AltGr e richiesta opzionale SET_IDLE rifiutata.
- **17 live + 10 installati senza ISO**, 236 asserzioni input; 27 verifiche assi X/Y indipendenti e controllo negativo: PASS.
- Installer BIOS e avvio incrociato UEFI, persistenza documenti, Recovery locale sigillato e ripristino effettivo con mouse/tablet: PASS.
- Tastiera italiana: 4 profili applicazioni, 4 Recovery e 5 client aggiuntivi: PASS.
- Dieci profili di aspetto, risorse reali di filesystem/atlas, Recovery procedurale tranne Terminale, associazioni sfondi, versione 10, wordmark, Mission Control e menu: PASS.
- **29/29 regressioni kernel**; kernel delle cinque sonde identico al kernel di produzione. Le sonde non sono un browser.

## Perimetro delle prove

L'automazione ottica BIOS usa tasti ordinari tenuti 300 ms e impulsi di selezione da 120 ms per il polling firmware emulato. Non è una prova di throughput della tastiera. I precedenti tentativi con temporizzazioni corte e gli esperimenti non distribuiti sono descritti in `diagnosis/INPUT-TIMING.md`; non sono conteggiati fra i PASS finali.

OpenDuet espone il FAT ottico BIOS come volume UEFI: l'etichetta testuale di origine non identifica da sola il dispositivo host. La connessione esclusiva della ISO è verificata con QMP e l'immagine Recovery viene verificata dalla shell.

**ASRock fisico non riprovato: mouse fermo nel live e in Recovery nell'ultimo riscontro; soluzione hardware non confermata.** Nessuna certificazione implicita del selettore firmware con PS/2 fisica. Safari/WebKit sospeso e non incluso.

L'esito del deploy e dei download remoti è attestato separatamente in `github-release-verification.json`.

## Deploy verificato

v10 normale/Latest pubblicata, release ID 453337659 invariato. Tag sorgente `b5c053ea34fd36681b9cfec7cc549698b744bede`. ISO e checksum scaricati nuovamente e confrontati: identici ai file collaudati. Due soli asset finali, nome ISO pulito; quattro release storiche ancora Pre-release. Ricevuta completa in `github-release-verification.json`.
