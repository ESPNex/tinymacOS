# Collaudo · tinymacOS 1.3 «Orizzonte»

**[ISO su Google Drive](https://drive.google.com/uc?id=16fx2iSZpYGB7Zflvncaq726rc-CMmzTb&export=download)** · `tinymacOS-1.3-x86_64.iso` · **53.024.768 byte**. SHA-256: `04227355a3d08d345cbc97f63af531711ce21029d957a0e2f399e54df72bbb72`. Dimensione e SHA-256 restituiti dal caricamento e dalla successiva ricerca del file su Drive coincidono con la ISO locale. La copia di staging non è rimasta nel workspace.

**70 controlli su questa ISO:** 58 prove di integrazione, 3 sulle destinazioni protette, 5 su account/compatibilità e 4 supplementari sul desktop installato. A parte, primitive SHA-256/PBKDF2 confrontate con `hashlib`. Riferimento: QEMU x86-64 10.0.13 TCG, Q35, OVMF, una vCPU, 512 MiB, VGA/GOP 1280×800, ICH9 AHCI, `qemu-xhci`, `usb-kbd`, `usb-mouse`; niente i8042, controller IDE legacy o rete. Non è una certificazione per hardware fisico o per altri hypervisor.

## Prova di installazione

1. Avvio da CD, con un disco SATA vergine da 96 MiB. Creazione account, cartella e documento dal desktop. Doppio clic sull’icona **Installa tinymacOS**; scelta del disco, riepilogo, consenso separato. Prima della spunta, *Installa* non avvia alcuna scrittura.
2. Scansione di tutti i settori; copia ESP e dati utente; rilettura prima della finalizzazione e spegnimento ACPI S5. A VM spenta un parser host indipendente ha verificato i CRC delle **due** GPT (header e array), MBR protettivo, confini delle partizioni e contenuti TinyFS. Tutti i 20 MiB dell’ESP corrispondevano al payload FAT sul CD.
3. Avvio di un **nuovo processo QEMU**, con nuove variabili OVMF, stesso disco e **nessun dispositivo ottico**. OVMF ha selezionato la porta SATA del disco, non il CD. Il kernel ha caricato la partizione dati collegata all’ESP, presentato il login e accettato la password originale. `cat Desktop/bozza.txt` ha restituito `Documento installato`. Nuovi file creati dal sistema installato sono sopravvissuti a un ulteriore avvio a freddo senza CD.
4. Un secondo disco privo di tabella partizioni ma con dati a 29 MiB è stato rifiutato durante la scansione. Prima e dopo: **intera immagine da 64 MiB identica**, SHA-256 `8d3834cb363c8e7649318a4d424180b4458c1ccc0f631e50bca96ac39dfa5415`. Provati anche nessun disco e avvio dell’assistente dal sistema già installato.
5. Migrazione con il vero CD 1.2.2: account e documento creati sul vecchio disco dati; importazione da CD 1.3, password errata rifiutata e originale accettata, copia su un secondo disco SATA, documento recuperato senza CD/RNG. **Gli 8 MiB originali non sono cambiati.** Inclusa prova aggiuntiva con il vecchio formato preferenze a tre byte.

## File visibili, UI e input

| Oggetto effettivamente letto | Byte | SHA-256 |
|---|---:|---|
| `/System/Kernel/kernel.elf` | 10.473.920 | `ad7c1012f8a27562c649049d950200dd0bd52b7e8e9583ff9d6de0ef784eaa16` |
| Loader `BOOTX64.EFI` | 196.772 | `d1f60488203e00294c27d1ca4edf00c9319b2ed951469a3a67a0cdc920e0ddd0` |
| ESP installata | 20.971.520 | `975752961133c314934364e4979a4a7cf3788c6d4167d66b16dddc5322846f79` |

- I due binari su FAT El Torito, ISO ed ESP corrispondono. Finder ha letto e verificato i digest dei file, non la memoria di esecuzione; testati navigazione all’ultima pagina, `shasum`, sola lettura e assenza di `Sorgenti/`, sorgenti e mappa del linker.
- Il PCI del guest non espone una classe IDE legacy. Il driver post-UEFI ha enumerato e usato tastiera e mouse USB anche al login. Dal Finder e dalla scrivania: creazione cartella/file, rinomina dell’elemento colpito dal clic destro, ispezione del kernel. In TextEdit: copia, incolla, salva dal menu contestuale, risultato riletto da Terminale. Sul Dock: apertura app. Sul titolo: contrazione e ripristino reali.
- Cinque campioni di colore del pannello Impostazioni: `#FFFFFF`, `#29292E`, `#F9FDFF`, `#17332C`, `#382A2D`. Per i tre nuovi temi i pixel di sfondo della VM corrispondono ai rispettivi QOI. Un nuovo boot ha ripristinato Rame vespertino, sia nella configurazione TinyFS sia sullo schermo.
- **Mela nel menu:** sagoma fornita, altezza di progetto ridotta da 20 a **16 px** nella barra da 34 px; nucleo quasi bianco misurato di 14 px. **Boot:** 36 fotogrammi intermedi con logo identico pixel per pixel alla maschera 94×108 e barra distinta sotto di esso. **Login:** tutte e 12 le fasi osservate durante creazione/rifiuto/accesso; una sola fase in movimento ridotto.

## Riprodurre

```sh
./build.sh
TMC_CPU=max,rdrand=off,rdseed=off TMC_RNG=none python3 tests/integration.py --evidence /tmp/tmc13-qa
python3 tests/disk_guard.py --evidence /tmp/tmc13-guard
python3 tests/desktop_extra.py --evidence /tmp/tmc13-ui
python3 tests/salt_compat.py --evidence /tmp/tmc13-salt \
  --previous /percorso/tinymacOS-1.2.2-x86_64.iso \
  --previous-launcher /percorso/kit-1.2.2/tools/run-qemu.sh
python3 tests/crypto.py
```

Omettere entrambe le opzioni `--previous*` per saltare solo la migrazione con il vecchio CD. Servono spazio temporaneo per più dischi, Python e Pillow, oltre alla toolchain QEMU/OVMF. Se si usa `TOOLROOT`, esportarlo anche per le prove. Le interazioni sono input virtuali normali, i fotogrammi sono acquisiti con `screendump` e la verifica offline non monta filesystem dell’utente.

Misure e verbali: `tests/evidence/{results.txt,measurements.json,guard/disk-guard.json,salt/salt-checks.json,extra/desktop-extra.json}`. Nel progetto ci sono anche le schermate GOP e le trascrizioni seriali; il kit ZIP contiene codice, asset, fixture e verbali testuali, **non** immagini ISO, dischi personali, firmware o credenziali.
