# Macroblocco 1 — registro dei risultati

**INCOMPLETO. Nessun passaggio al macroblocco 2, nessuna release e nessun cambio di versione.**

Unico riferimento: il PDF fornito dall’utente, `tinymacOS/docs/piano.pdf`, SHA-256 `fe66291ae6368af7abbec48877f4086ed4c76180469c2e87873abd6aa175a86f`. Il vecchio piano è eliminato; questo è il registro dei test, non un nuovo piano. Le ricevute dei precedenti M0–M3 sono storiche e non chiudono i macroblocchi del PDF. Lavoro M4 congelato e non riapplicato.

## Risultati verificati della nuova implementazione

### Nome reale della home

- La directory viene realmente rinominata con il nome dell’account dopo autenticazione, non semplicemente rietichettata nel Finder. Nessuna copia dei dati e nessuna seconda directory `User`.
- `/Users/User` resta un percorso di compatibilità interno. L’elenco `/Users`, i percorsi del Finder e `pwd`/`$HOME` del Terminale mostrano il nome effettivo. Il nome è ricavato dall’inode realmente esistente, non solo dall’etichetta account.
- Migrazione metadata-only sia per filesystem live legacy sia per FS2 installato, con conservazione degli ID, dei file aperti, dei contenuti e dei record account. Nomi UTF-8 con spazi/accenti verificati: `Elena Ròssi`. Rifiuto di nomi invalidi e collisioni senza unire/sovrascrivere cartelle.
- Gli snapshot mantengono i propri nomi storici. Il vecchio percorso HOME all’interno di uno snapshot viene risolto esclusivamente usando l’account di quello snapshot, mai l’identità live. Ripristino di snapshot pre/post-migrazione, rimontaggio a freddo e sblocco FV003 verificati.
- PASS normale + ASan/UBSan: `home-identity-host.log` (FS2 e FV003 reali), `home-legacy-host.log` (anche cifratura legacy e settori payload invariati), `ui-path-host.log` e `fs2-runtime-host.log`.
- PASS guest: setup live, importazione tramite installer GUI, avvio NVMe senza ISO, `pwd` e `$HOME` reali, ripristino CoW, reinstallazione interna da Recovery e nuovo avvio con account/documenti/snapshot intatti. L’intera partizione dati conserva lo stesso SHA-256 durante la reinstallazione. Ricevute `install-home-recovery-result.json` e log associato.
- Controllo indipendente in sola lettura del disco creato dal guest (`tests/home_inspect.c`): esattamente una home `Fs2Install`, nessuna directory fisica `User`, account e documenti importati/ripristinati validi. PASS in `home-inspect.log`. La variante UTF-8 è collaudata nei test host, non nella digitazione del guest.
- `installed-home-terminal-crop.png` è un ritaglio rettangolare lossless del vero screenshot guest: mostra il percorso `/Users/Fs2Install` e i documenti dopo reinstallazione. Coordinate e hash dell’originale in `installed-home-crop.json`; nessun pixel aggiunto o sostituito.

### OpenCore e sicurezza delle variabili EFI

- Corretto il caso di scrittura BootOrder riuscita ma rilettura fallita/non corrispondente: viene tentato il ripristino e verificato l’ordine precedente prima di eliminare l’eventuale nuova voce. Se il rollback non è verificabile, la voce referenziata resta presente. Test con guasti iniettati: PASS normale + ASan/UBSan, `boot-default-current.log`.
- Aggiunto uno strumento EFI esplicito nel menu OpenCore per ripristinare l’ordine originale. Conserva backup e voci Boot####, non modifica file Microsoft; un flag persistente impedisce la ripromozione automatica dopo il ripristino. Riabilitazione disponibile solo su scelta dell’utente nello stesso strumento.
- PASS guest dello strumento EFI installato, avviato con tastiera virtuale reale: l’ordine originale viene ripristinato byte per byte e il flag persiste. Decoder indipendente delle variabili OVMF verifica anche la voce dedicata OpenCore in prima posizione dopo un avvio nativo. `opencore-tools-result.json`, log e screenshot `uefi-original-boot-order-restored.png`.
- **Lacuna funzionale ancora presente:** la registrazione EFI è eseguita dal loader al primo avvio nativo del sistema installato, NON dalla fase finale dell’installer. Con Windows già prioritario non è dimostrato né garantito che quel primo avvio avvenga automaticamente. Non è quindi soddisfatto integralmente «all’installazione sostituire Windows Boot Manager».
- Nessun Windows reale presente nei test: né il chainload Microsoft né la convivenza su firmware fisico sono certificati. Le partizioni estranee dei test sono sintetiche.

### Partizionamento e capacità

- PASS anche il caso del PDF: disco sintetico da 12 GB con gap allineato da **10.000.269.312 byte**, mostrato come **10,0 GB**. Installer GUI assegna l’intero gap al container v2; FS2 occupa la porzione dati risultante. Avvii nativo/Recovery senza ISO, tre viste delle capacità e controllo indipendente di ogni byte estraneo: PASS, `gap10-current-512-result.json` e log associato. Non resta un gap inutilizzato da circa 10 GB.

- PASS della nuova ISO nel guest 3 TiB/4Kn: installazione GUI nel gap reale da 2,5 TiB, geometria GPT/FS2 verificata indipendentemente, avvio nativo e Recovery senza ISO, capacità coerenti in Installer/Utility Disco/Terminale. Verificato ogni byte delle regioni estranee tramite SEEK_DATA/HOLE e settori allocati. `capacity-current-4096-result.json`.
- PASS regressioni host normali + ASan/UBSan: `partitions-current.log`, `large-disks-current.log`, test Windows-gap inclusi in `foundation-current.log`. Coperti GPT/MBR, canary di scrittura, preservazione di partizioni estranee, 1 TB/16 TiB, rifiuto GPT degradate e capacità superiori al limite.
- I container hanno un allineamento documentato di 1 MiB: una coda inferiore a 1 MiB non equivale al difetto dei circa 10 GB interamente non assegnati. Il formato v2 è valido per span a 32 bit; il v3 serve agli span più grandi. Il primo tentativo del nuovo fixture piccolo assumeva erroneamente v3 e nessun arrotondamento: fallimento dell’asserzione del fixture, non un PASS e non corretto modificando il filesystem.

### Video UEFI

- Confermati il filtro GOP corretto e la codifica BootConfig delle larghezze non multiple di otto, preservando i vecchi byte: `gop-current.log` normale + ASan/UBSan. Vengono accettate soltanto modalità realmente esposte dal firmware e compatibili con stride/layout/apertura supportati.
- Il loader cerca prima il GOP dell’uscita console, con fallback al protocollo disponibile.
- Aggiunto nel menu OpenCore uno strumento EFI di diagnostica **in sola lettura**: enumera le modalità GOP, segnala la compatibilità e l’eventuale EDID senza inventare o forzare modalità. Con più GPU l’EDID di fallback può appartenere a un’altra uscita: l’avviso è esplicito.
- PASS avvio reale dello strumento installato in OVMF, enumerazione e prompt finale, senza modificare BootOrder/backup: `opencore-tools-result.json` e screenshot `uefi-video-diagnostic*.png`.
- **GeForce 310 via HDMI e monitor fisici non disponibili.** Nessuna prova che il loro firmware esponga risoluzioni maggiori; nessun driver NVIDIA nuovo implementato. QEMU non chiude questo requisito.

## Build e preservazione

Build completa BIOS/UEFI/Recovery/app/strumenti EFI: **PASS**, `build-current.log`. ISO esclusivamente di sviluppo, non pubblicata come release:

`234bad9e9f40c75bf965884a2b396c2bb74d32f2978c239045fdd6d851d282eb`

Hash e dimensioni degli artefatti: `home-artifacts.json`; impronte dei sorgenti e dei fixture: `home-source-manifest.json`. Le ricevute `artifacts.json`, `build.log`, `source-manifest.json` e gli altri log senza suffisso corrente appartengono alla precedente ISO e restano storici.

PASS foundation corrente, contratto M0/USB canonico, asset/font bloccati, audit stringhe e overlay. Nessun nuovo stack USB, font sostituito, Safari reintrodotto o patch M4 riapplicata. Il primo lancio foundation ha incontrato l’inventario stringhe cambiato durante le modifiche; rieseguito dopo l’aggiornamento, PASS. Il primo lancio partizioni senza TMC_BUILD è stato rifiutato prima dei test; rieseguito con il build esterno, PASS. I primi tentativi del driver di test EFI premevano Esc troppo presto oppure Invio dopo una scelta numerica già immediata; corretto soltanto il driver di input, poi strumenti verificati con successo.

**Verdetto: macroblocco 1 NON COMPLETO.** Restano l’attivazione OpenCore direttamente dal completamento dell’installazione, l’accettazione del chainload Windows e il risultato sul vero hardware GeForce/HDMI. Non vengono trasformati in PASS mediante test sintetici o una semplice modifica di stato.

Tutti i processi di build/test di questa lavorazione sono conclusi; nessuna prova resta in esecuzione al momento della pubblicazione dei sorgenti.
