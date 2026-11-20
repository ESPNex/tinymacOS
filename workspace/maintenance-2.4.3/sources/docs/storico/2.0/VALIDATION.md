# Riviera 2.0 · collaudo del supporto consegnato

**18 novembre 2026 · build 20A1.** Sono stati provati la ISO finale e i dischi prodotti dal suo installatore. Non sono conteggiati esiti delle versioni precedenti. Le prove di scrittura ed errore sono state eseguite su dischi virtuali dedicati o cloni; nessun disco personale.

| Download | Byte | SHA-256 rilevato su Drive e in locale |
|---|---:|---|
| [ISO UEFI live/installazione](https://drive.google.com/uc?id=1IpyKR0xBkJZBW7ftAFV7MkPeymaNA9cA&export=download) | 55.191.552 | `64bdf2af24bea3d025798242d00d57ba02a2328bdae7778be1a2eda9812b9523` |
| [BaseSystem.img autonomo](https://drive.google.com/uc?id=1B1cdGg1jKvDGr_Hzy4gxgacbQfABg0IH&export=download) | 4.194.304 | `8829f851190b7ad995fb69d8e968347c54c7ed3389eab2c5a04df83485b94da9` |

Due build in directory esterne indipendenti hanno prodotto **gli stessi byte** per ISO, BaseSystem, KernelCache, entrambi i programmi UEFI, la ESP e i due cataloghi. L’IMG su Drive è identico a `/Recovery/BaseSystem.img` sulla ISO e alla partizione Recovery del disco installato. Upload diretto dai file esterni; nessun passaggio dei media nel workspace. Un risultato per nome in Drive, `canDownload=true` per l’account collegato. Non sono stati cambiati i permessi di condivisione. La 1.3 mantiene ID, dimensione e hash originali.

## Cosa è stato provato

**Banco:** QEMU 10.0.13, OVMF UEFI non Secure Boot, x86-64 Q35, TCG, 1 vCPU, 512 MiB, GOP 1280×800, disco SATA/AHCI da 96 MiB, xHCI con tastiera e mouse USB, i8042 assente, nessuna rete. Per l’ingresso nella UI sono stati usati tasti e clic normali. I test hanno letto i pixel dal display VM e confrontato indipendentemente strutture e contenuti a VM spenta. Un’eventuale riga seriale `BdsDxe` è del firmware, non un log di debug del guest.

| Percorso | Risultato osservato |
|---|---|
| Avvio e volumi | 35 livelli distinti della barra sotto la mela; creazione dell’account, scrivania e libreria con 13 app su live. Il `KernelCache` letto dal supporto racchiude un ELF64 con lunghezza e SHA-256 verificati. |
| Installazione e rimozione del CD | Doppio clic USB su *Installa tinymacOS*, scelta disco e conferma, copia, rilettura, spegnimento. Nuova NVRAM, nessun CD: login con lo stesso account. 12 app; nessun installer in Finder/Cerca/Libreria, via F11, `open -a`, nel catalogo o come immagine eseguibile sul disco intero. Un disco apparentemente vuoto con un documento a 33 MiB è stato rifiutato: SHA-256 di tutti i suoi byte invariato. |
| Bundle e strumenti | Manifest `Info.plist` analizzati, Mach-O statici x86-64 con `LC_MAIN` in segmento eseguibile e senza W+X. In Finder: clic destro → *Mostra contenuto pacchetto*; aperti il plist e l’eseguibile di Note. I due SHA-256 mostrati dal Finder sono identici a quelli dei file letti dal catalogo in modo indipendente. Lancio di Note dal suo bundle. |
| Terminale | Programmi in 26 percorsi Unix. Verificati byte esatti su disco dopo `cp`, `mv`, `rm`, `mkdir`/`rmdir`, stdin, pipeline, `>`/`>>`, `grep`, `sort`, `head`/`tail`, `wc`, SHA, variabili, percorsi con spazi, cronologia e completamento. `diskutil`, `ps`, `whoami`, `sw_vers`, `pwd`, `date` e l’alias `/Volumes/Macintosh HD` usano il sistema attivo. |
| Kernel e processi | 585 campioni atomici dei registri della vCPU: CS `0x08`/CPL 0, CS `0x23`/CPL 3, quattro radici di paginazione distinte osservate in Ring 3, WP e NXE attivi. Nessuna app diagnostica distribuita. La mela coincide pixel per pixel con il raster fornito e sono state osservate tutte e 12 le fasi radiali al login. |
| Documenti | Un testo salvato in TextEdit è stato copiato dal Terminale e confrontato byte per byte a VM spenta. Note e Memo creati sulla live, inclusi nell’installazione, riaperti dal disco. Provati autosalvataggio, ricerca, annullamento e conferma dell’eliminazione; testo e colore del memo confrontati nel framebuffer e nei file personali. |
| Copia e ripristino dati | Utility Disco ha eseguito SOS e salvato una copia locale. Modificato un documento, riavviato nella partizione Recovery, ripristinata la copia, selezionato il disco di avvio e rifatto il login: ripristinato il testo **precedente**, anche alla rilettura offline. La copia è sullo stesso disco, non su un supporto di backup esterno. |
| Riparazioni reali | Danneggiato il CRC della GPT primaria *su un clone*: SOS da USB l’ha ricostruita dalla secondaria. Corrotto il KernelCache installato: il loader è entrato in Recupero dal disco. Reinstallazione senza rete e senza CD, seguita da riavvio UEFI `BootNext`: **tutta la ESP** corrisponde di nuovo alla sorgente e **il volume dati è invariato**. |
| Conferme e integrità | «Inizializza» ha rifiutato una parola sbagliata, accettato `INIZIALIZZA`, cancellato vecchio account, documenti e copia locale; il successivo setup ha registrato un account nuovo. Altri due tentativi di ripristino con copia dati e sorgente di sistema alterate si sono fermati **senza modificare alcun byte del disco**. |
| Recupero indipendente | Il tasto R sul CD carica il programma dall’immagine FAT. Anche la sola BaseSystem via USB, senza disco o CD e con NVRAM nuova, mostra le cinque scelte. UI ferma a parità di input; `sw_vers` e data eseguiti nella shell UEFI. Nessun WindowServer o app desktop nell’immagine. |
| Console host | Entrambe le modalità via WebSocket/RFB mostrano le vere schermate «Crea un account» e «Scegli come proseguire». Entrambi i download HTTP della console, letti in streaming senza scrivere copie, hanno gli stessi byte/digest delle immagini di build. |

**36 gruppi di verifiche su supporti e VM, più 4 sulla console host.** Le evidenze sono in [`tests/evidence/riviera/`](../tests/evidence/riviera/): `build.json` e `drive.json` identificano i supporti; `live.json`, `disk.json`, `guard.json`, `apps.json`, `integrity.json`, `runtime.json`, `cpu-login.json`, `console.json` raccolgono gli esiti. Le immagini numerate sono acquisizioni GOP della VM, non rendering ricostruiti. SHA-256 su 9 lunghezze e PBKDF2-HMAC-SHA256 su 4 conteggi confrontati anche con `hashlib` sull’host.

## Ripetere il collaudo

Da Linux amd64: toolchain e launcher come nel README. Le suite black-box usano Python 3.11+, Pillow, NumPy e OpenCV; Tesseract con `eng` stampa il testo dello schermo in caso di errore. `OCR_ROOT` è facoltativo e serve solo per un’installazione ospitata in un prefisso non di sistema. Non servono root, rete nel guest, montaggio di immagini o password predefinite sulla ISO.

```sh
export TMC_BUILD=/var/tmp/tinymac-riviera
./build.sh
work=$(mktemp -d /var/tmp/qa-riviera-XXXXXX)
python3 tests/riviera_live.py "$work/live"
python3 tests/riviera_guard.py "$work/guard"
python3 tests/riviera_disk.py "$work/live/sata/system.raw" "$work/disk"
python3 tests/riviera_apps.py "$work/live/sata/system.raw" "$work/apps"
python3 tests/riviera_integrity.py "$work/disk/installed.raw" "$work/integrity"
python3 tests/riviera_runtime.py "$work/live/sata/system.raw" "$work/runtime"
python3 tests/crypto.py
# Opzionale, con tools/console/server.py in esecuzione all’indirizzo indicato:
python3 tests/riviera_console.py http://127.0.0.1:8080 "$TMC_BUILD" "$work/console"
```

In alternativa, `python3 tests/integration.py` esegue in sequenza le suite native sopra, esclusa la console opzionale. Non eredita le destinazioni `DISK` o `LEGACY_DATA`. I dischi di prova vengono lasciati nella nuova directory esterna stampata all’avvio.

## Portata

Questi risultati riguardano la configurazione virtuale indicata, non Mac o PC fisici, Secure Boot, NVMe, rete, riparazioni universali o applicazioni per Apple macOS. TinyFS non è APFS; i digest rilevano corruzione, non sostituiscono firme digitali o cifratura. Non sono state effettuate prove esaustive di interruzione dell’alimentazione in ogni fase; conservare sempre una copia esterna del disco raw a macchina spenta.
