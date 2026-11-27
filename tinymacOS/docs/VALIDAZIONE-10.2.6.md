# tinymacOS 10.2.6 Tramontana — validazione 30C1

Una sola ISO BIOS+UEFI. 9 suite host e 17 suite guest sulla stessa ISO finale,
più confronto negativo sulla ISO 10.2.5/30B21. Test QEMU/sintetici, non una
certificazione della nuova build su PC fisico o del disco Windows dell'utente.
L'utente ha confermato il mouse fisico della base 30B21.

## Risultati

- Percorso USB nativo alternativo rimosso; controlli preventivi sui sorgenti e
  simboli dei due kernel. USB core/HID canonical obbligatorio, worker attivo.
- Otto profili USB: BIOS/UEFI, due mouse xHCI, EHCI, hub xHCI, tablet xHCI,
  tastiera senza PS/2; cursore, input, teardown e riconnessione verificati.
- FS2: vera installazione GUI, import account/file, cold NVMe boot, snapshot,
  Recovery/reinstall con hash completo dei dati invariato.
- Drag-and-drop: mouse relativo reale, file/directory, Finder/desktop/sidebar,
  Ctrl-copia con nome non sovrascrivente, percorso quotato in Terminale che non
  esegue senza Enter, menu Taglia/Copia/Incolla, appunti, pulizia al lock e
  persistenza dopo riavvio. Sorgenti recuperati dalla 30B18, non riscritti a caso.
- Nuova copertura PMBR incoerente: prova host normale e ASan/UBSan, lunghezza
  obsoleta/saturata, sola firma assente e combinazioni. Preflight/cancel zero
  scritture; ripristino con consenso, vecchio MBR salvato, identificativo disco,
  altre voci, GPT preesistenti e partizioni occupate preservati. GPT degradate,
  ibride o ambigue ancora bloccate. Due prove guest separano copertura e 55AA,
  con conferma GUI, annullamento, verifica bordi e cold boot NVMe.
- Recovery ottica senza installazione: mouse relativo e guida al desktop live,
  nessuna modifica alla fixture Windows-like. Nessuna accusa generica di GPT corrotta.
- NVMe 4Kn: installazione, cold boot, Recovery/reinstall, conversione e
  disattivazione cifratura sullo stesso extent; account/file/snapshot e tutti
  i byte delle partizioni pubbliche preservati. Nessuna chiave privata nelle immagini archiviate.
- Avvio ottico SATA UEFI e BIOS/OpenDuet, account/Terminale/documento.
- 18 finestre vere: sulla vecchia 30B21 le pubblicazioni grafiche identiche
  aggiornano la sequenza, sulla 30C1 vengono riusate (raster esclusi).
  Tick schedulati del desktop in circa 12 s: 1651 -> 513 nella prova QEMU.
  Non è una promessa di guadagno costante su hardware reale. Mouse e Terminale
  ancora utilizzabili; nessuna accelerazione GPU dichiarata.
- Audit estratto ISO: font UI/Mono e panic bloccati, versione, kernel live e
  installato identici e Recovery corrispondente al binario controllato.
- Suite host: canonical ABI, canonical usbhid, xHCI, native4k install,
  installer flow, Windows gaps, input maintenance, FS2 installer effettivo,
  deduplicazione comandi finestre. Sanitizzatori secondo le singole ricevute.
- Source-sync: 26 integrazioni OS e 11 ABI; cleanup: 972 input protetti con
  eccezioni autorizzate esplicite. Font e sorgenti Safari preservati.

## Conservazione e limiti

Circa 34 MB liberati prima delle nuove evidenze. Font UI/Mono forniti, FS2,
4Kn/cifratura, visibilità EFI/Recovery e menu/appunti conservati. Nessun RAW,
MSR o NTFS riclassificato libero. La normalizzazione PMBR non ripara GPT
corrotte: richiede due copie valide e identiche, spazio realmente non allocato
e consenso. Il contenuto PMBR preciso del PC dell'utente non è stato acquisito.

Helper indipendente dal sistema mantenuto byte-identico dalla 30B18 (vecchio
README interno 10.2.5), non un nuovo test di boot. ISO non USB-DD ibrida.
Safari/WebKit completo non incluso. Backup prima di installare.

- ISO: `tinymacOS-10.2.6-x86_64.iso`
- Byte: 329580544
- SHA-256: `2b702ce411b1e2cb9e5427f3e2ecdf1fc0849ee834b48871e73aa40c78c31149`
- Note: `tinymacOS/docs/NOTE-10.2.6.md`
- Ricevute: `workspace/local-10.2.6/verification.json`, `evidence.tar.xz`
- Pubblicazione e download verificati separatamente in `publication-readback.json`.
