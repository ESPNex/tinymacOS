# tinymacOS 10 · Tramontana — 30A1

**PUBBLICATA E VERIFICATA:** [v10.0](https://github.com/ESPNex/tinymacOS/releases/tag/v10.0), build **30A1**, Release normale e Latest. ISO riscaricata dopo la pubblicazione e SHA-256 verificato. Tutte le quattro release precedenti sono ora Pre-release. Ricevuta: `github-release-verification.json`. Tutte le prove seguenti sono nuove e legate alla SHA della 30A1, non ricevute riutilizzate della 21B9. Il tag sorgenti `v10.0` punta al commit `9a7bcd6984f3bad83112736b3f7cc68091ae2f77`; nessun tag precedente è stato spostato.

## Aspetto e risorse

- Nuovo wallpaper Tramontana non procedurale, realizzato con generazione nativa di immagini. Originale conservato nei sorgenti e risorsa QOI 1280×800 installata in `/System/Library/Desktop Pictures/Tramontana.qoi`.
- Icone importate dallo ZIP fornito dall'utente, scaricato esclusivamente fuori dal workspace. Per ogni nome esatto viene selezionata la variante con data più recente. Manifest con nome, percorso originale e SHA-256 incluso nel filesystem.
- 35 slot grafici usano immagini. Atlas con sei risoluzioni: 16, 24, 32, 48, 64 e 128 px. PNG originali nei CoreTypes e risorse `AppIcon.png`/`Icon<size>.png` nei bundle delle app. Dock, Finder, libreria e scrivania condividono lo stesso packer deterministico; glifi UI, cursore e branding conservano i rispettivi asset dedicati.
- **Eccezione richiesta:** icona installer originale procedurale dedicata a Tramontana, non una vecchia icona dell'installer riutilizzata.
- Tema Tramontana e tredicesimo sfondo predefiniti; Blu, Azzurro e Riviera hanno palette complete distinte sia in chiaro sia in scuro. Galleria e Impostazioni espongono tutti i tredici sfondi.
- Informazioni: “tiny” conserva la faccia regolare e gli stessi 72 px di avanzamento; “macOS” usa una faccia autentica weight 600, non bold. Prime dieci facce conservate bit per bit; euro presente in tutte e undici. Licenza OFL inclusa.
- Recupero: mela 24 px nativa allineata verticalmente al testo della barra e titolo finestra ridotto.
- Identità 10.0 / 30A1 aggiornata in shell, app, bundle, volumi, installer, Recupero e documentazione iniziale.

## Manutenzione conservata

Tastiera fisica esclusivamente italiana ISO: UTF-8, accenti, euro, AltGr e punteggiatura in login/app/Recupero. PS/2, EHCI HID, UHCI companion, xHCI e hub USB2; decoder degli assi mouse indipendenti da descrittore HID validato. Nessun pannello diagnostico F11 al login. Il collegamento installer sulla scrivania e F11 sul desktop live restano disponibili.

Conservati avvio CD BIOS/OpenDuet/OpenCore e UEFI nativo, installazione GUI, disco installato BIOS+UEFI, Recupero locale sigillato, VM/MMU multi-CPU, TLS per-thread e sincronizzazione nativa. Sono funzionalità sperimentali, non una compatibilità POSIX/macOS completa.

## QA e pubblicazione

I report `assets-result.json` e `appearance-result.json` controllano risorse realmente impacchettate e framebuffer reale: non mockup. Il gate `collect_qa.py` deve verificare la candidata, l'identità, le prove input live/installato, italiano app e Recupero, Mission Control, installazione, recupero locale, ricostruzione indipendente identica e kernel delle sonde uguale a quello di produzione.

`final-qa.json`, quando PASS, è il riepilogo completo. `github-release-verification.json`, solo dopo il deploy, registra release/tag/asset e download remoto verificato. La presenza di questo documento, da sola, non certifica né PASS né pubblicazione.

## Limiti

- **Nessuna prova sul PC fisico ASRock Q1900M/J1900.** QEMU non dimostra che il mouse fisico sia ora corretto né certifica compatibilità universale.
- OHCI, hotplug USB generale, hub SuperSpeed e HID universale non implementati.
- La ISO non è un'immagine USB DD ibrida. Le prove BIOS sono CD virtuale e disco SATA virtuale installato; nessuna nuova prova Windows/Rufus/hardware reale.
- Installare soltanto su un disco virtuale vuoto dedicato. La nuova ISO non migra automaticamente i vecchi sistemi installati.
- Safari/WebKit reale resta sospeso e non è incluso. I test kernel/backend non sono test HTML/CSS/JavaScript/HTTPS di un browser.
- Icone dall'archivio dell'utente, non una dichiarazione di originalità o licenza delle icone Apple/terze parti. Repository e download restano privati. Font semibold e boot vendor conservano le loro licenze.

## Risultato finale 30A1

| Gate | Risultato |
|---|---|
| Build nativa + BIOS/UEFI, ricostruzione pulita indipendente | ISO identiche bit per bit |
| Input live / installato senza ISO | 17 + 10 profili, 236 verifiche PASS |
| Italiano nelle app / Recupero | 4 + 4 profili PASS; altri 5 client PASS |
| Kernel/runtime | 29/29 PASS; kernel delle 5 sonde uguale alla produzione |
| Aspetto | 8 combinazioni tema/modalità PASS |
| Risorse | 35 slot, 6 risoluzioni, PNG nel filesystem e date più recenti verificati |
| Installer BIOS, disco autonomo BIOS/UEFI, ripristino locale sigillato | PASS |
| Mission Control | 6 cicli, nessun pixel residuo/alone; mela allineata entro 1 px |
| Recupero | Mela/testo centrati esattamente allo stesso pixel; titolo ridotto verificato |
| Sorgenti di produzione | 514 file congelati confrontati dopo il collaudo |

File: `tinymacOS-10.0-OpenCore-Installer-x86_64.iso`  
Dimensione: **224.966.656 byte**  
SHA-256: `a1d9e28d31bbb5c2c29c858d633ac7b411eafb1a466d186c0f0dd4cf22f60461`

Deploy completato: Release ID `453337659`, asset verificati sotto il nome definitivo dopo la pubblicazione. Tutte le release anteriori (`v2.4.5`, `v2.4.4-oc2`, `v2.4.4-oc1`, `v2.4.4`) sono Pre-release. Nessun hardware fisico è stato verificato.
