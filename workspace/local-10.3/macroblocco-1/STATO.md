# Macroblocco 1 — registro dei risultati

**CHIUSO SU INDICAZIONE ESPLICITA DELL’UTENTE: «chiudi il macroblocco 1 ora 2». Accettazione fisica incompleta, non trasformata in PASS. Nessuna release e nessun cambio di versione.**

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
- **Lacuna di `ef14471` ora corretta:** OpenCore viene promosso direttamente alla fine dell’installazione, dopo rilettura del sistema e commit delle tabelle. Non è più necessario il primo avvio manuale per registrarlo. La verifica corrente, eseguita prima di qualsiasi avvio nativo e poi su avvii senza ISO, è descritta nella sezione seguente.
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

## Attivazione EFI al termine dell’installazione — verifica corrente

- Il loader conserva un ponte fisico limitato ai servizi variabili EFI. Sono verificati tutti i livelli delle page table originali: pagine convenzionali/reclamabili, radici fuori limite e strutture mancanti disabilitano il ponte prima di qualsiasi chiamata. Handoff append-only con magic: invariati gli offset precedenti, incluso SIP.
- Nessuna syscall generale di accesso NVRAM. Solo l’installer autenticato, fissato al BSP, costruisce il target della partizione appena installata e verificata. Accesso serializzato, stack firmware dedicato da 128 KiB, page table fisiche trattenute, stato x87/SSE salvato, ambiente FP pulito e stato originale ripristinato. Non vengono rese eseguibili pagine heap dell’OS.
- La voce usa un percorso UEFI standard HD/File con GUID GPT o firma MBR e coordinate nei veri blocchi logici nativi. Non punta al controller del supporto live. Il confronto dei suffissi HD/File evita voci duplicate quando il firmware espande il percorso fisico al primo avvio.
- La UI dichiara OpenCore principale **solo** dopo successo verificato. Firmware indisponibile o scrittura rifiutata producono l’indicazione di selezione manuale, non un falso esito positivo della preferenza.
- PASS **prima del primo avvio installato**, su disco nuovo e gap GPT con partizioni estranee: lettura indipendente delle vere variabili OVMF trova OpenCore primo, backup dell’ordine originale presente e target/coordinate corretti. Dopo spegnimento e rimozione ISO l’avvio è automatico; nessuna scelta manuale maschera il test. Verificati anche importazione della home, CoW, Recovery e reinstallazione senza modificare la partizione dati.
- PASS sulla **stessa ISO finale** per gap da 10.000.269.312 byte/512B e disco 3 TiB/gap 2,5 TiB/4Kn, incluse le tre viste delle capacità e ogni byte delle regioni estranee. Ricevute `runtime-fs2-install-guest-result.json`, `runtime-block1-gap10-512-result.json`, `runtime-m2-capacity-4096-result.json`.
- Il nuovo test di ripristino ha trovato un vero caso prima non coperto: l’ordine originale salvato dall’installer conteneva la voce del CD, che OVMF elimina quando il supporto viene rimosso. Corretto il ripristino: mantiene l’ordine originale dei dispositivi ancora presenti, salta soltanto le voci non più esistenti e non ricrea dispositivi fantasma. Se tutti i vecchi dispositivi sono scomparsi, rifiuta di sostituire l’ordine utilizzabile con uno vuoto. Il backup storico non viene alterato. Quando tutte le voci esistono, il ripristino resta byte-identico. PASS host normale/ASan/UBSan e strumento EFI reale dopo rimozione ISO: `runtime-boot-default-host.log`, `runtime-firmware-boot-host.log`, `runtime-block1-firmware-guest-result.json`.
- PASS della build finale completa e del controllo di immutabilità degli input durante la compilazione. Manifest compresso `runtime-build-inputs.sha256.gz`; sorgenti cambiati `runtime-source-manifest.json`; artefatti `runtime-artifacts.json`; log `runtime-build.log`. I test intermedi precedenti alla protezione FP sono stati superseduti; una sequenza è stata interrotta intenzionalmente e non conta come PASS complessivo.

**ISO finale esclusivamente di sviluppo:** `6f0f824d1ebe57036e1a82e4cd81523207f0057122e12589d2f9ab292e85da65`. Tutte e quattro le ricevute guest `runtime-*-result.json` riportano questo identico hash e PASS. `runtime-installer-completed-crop.png` mostra la vera UI dopo la verifica della preferenza, con ritaglio lossless documentato in `runtime-crop.json`.

`Diagnostica-video-UEFI.zip` contiene soltanto il piccolo programma GOP/EDID in sola lettura, avviabile da una chiavetta FAT32 già vuota; non è una release dell’OS né un installer. Serve a ottenere la prova sul PC GeForce/HDMI che questo ambiente non può eseguire. Hash in `diagnostic-artifact.json`; istruzioni e licenza nell’archivio. Non si considera la sola diagnosi equivalente a una risoluzione sbloccata.

## Build precedenti e preservazione

Build precedente di `ef14471`, BIOS/UEFI/Recovery/app/strumenti EFI: **PASS**, `build-current.log`. ISO esclusivamente di sviluppo, non pubblicata come release:

`234bad9e9f40c75bf965884a2b396c2bb74d32f2978c239045fdd6d851d282eb`

Hash e dimensioni degli artefatti: `home-artifacts.json`; impronte dei sorgenti e dei fixture: `home-source-manifest.json`. Le ricevute `artifacts.json`, `build.log`, `source-manifest.json` e gli altri log senza suffisso corrente appartengono alla precedente ISO e restano storici.

PASS foundation corrente, contratto M0/USB canonico, asset/font bloccati, audit stringhe e overlay. Nessun nuovo stack USB, font sostituito, Safari reintrodotto o patch M4 riapplicata. Il primo lancio foundation ha incontrato l’inventario stringhe cambiato durante le modifiche; rieseguito dopo l’aggiornamento, PASS. Il primo lancio partizioni senza TMC_BUILD è stato rifiutato prima dei test; rieseguito con il build esterno, PASS. I primi tentativi del driver di test EFI premevano Esc troppo presto oppure Invio dopo una scelta numerica già immediata; corretto soltanto il driver di input, poi strumenti verificati con successo.

**Chiusura su richiesta dell’utente; collaudo fisico non completato.** Restano l’accettazione del chainload su un Windows reale e il risultato sul vero hardware GeForce/HDMI. L’attivazione OpenCore direttamente al termine dell’installazione è ora implementata e verificata nei guest finali. Non vengono trasformati in PASS mediante test sintetici o una semplice modifica di stato.

Tutti i processi di build/test di questa lavorazione sono conclusi; nessuna prova resta in esecuzione al momento della pubblicazione dei sorgenti.

Ultima verifica sui sorgenti finali: foundation, BootOrder/guasti/ripristino e grandi dischi normali/ASan/UBSan **PASS** (`runtime-final-*-host.log`); gate EFI aggiuntivo con offset handoff invariati **PASS** (`runtime-firmware-boot-host.log`). Tutte le sequenze finali sono concluse. Le prove interrotte/intermedie non sostituiscono le quattro ricevute PASS della ISO finale.

L’utente ha autorizzato esplicitamente la chiusura e il passaggio al macroblocco 2. Questa decisione supera il precedente divieto di procedere finché ogni collaudo del primo blocco fosse verificato; non modifica retroattivamente gli esiti dei test e non certifica GPU/Windows. Checkpoint tecnico del blocco 1: `f33c1a231d1fe832fa1c17e1c4226384261a3684`.
