# tinymacOS 10.2.5 — validazione 30B19

Una sola ISO BIOS+UEFI, aggiornamento della release esistente. Non è una 10.3.

## Correzioni

- Rollback byte per byte di desktop/input alla 10.2.4; trasferimento file
  drag-and-drop disabilitato. Finder Taglia/Copia/Incolla e appunti Terminale conservati.
- GPT verificata prima del rifiuto della firma MBR assente. Ripristino proposto
  soltanto dall'installer 512 B con entrambe le GPT valide/concordanti, PMBR
  coerente e unico, nessuna voce residua/ibrida o sovrapposizione, slot libero e
  spazio realmente non allocato. Conferma esplicita del ripristino firma.
- Preflight e annullamento senza scritture; attivazione finale dopo verifica
  e MBR originale salvato. Normali scritture del bootloader e nuova voce GPT
  fanno parte dell'installazione: non si promette di scrivere due soli byte.
- RAW/MSR/EFI/NTFS mai convertiti in spazio libero. Diagnostica GPT distinta;
  intervalli positivi sub-MiB visualizzati come `<1 MiB`, non `0 MiB`.
- Font UI/Mono forniti, assenza fallback DejaVu, Terminale 8 px e FS2 preservati.

## Verifiche fresche sulla ISO finale

| Gruppo | Esito |
|---|---|
| Suite host storage/input/UI | 12/12 |
| Scanner GPT: firma assente, residui ambigui, GPT invalida/discordante, MSR/NTFS | normale + ASan/UBSan |
| Installer FS2 effettivo, firma assente, annullamento, preservazione, MBR salvato | normale + ASan/UBSan |
| Suite guest finali | 8/8 |
| GUI firma assente: firmware non ripara prima dell'OS, consenso, cancel zero scritture | PASS |
| Installazione nel vero gap sintetico da 10 GiB e cold boot NVMe senza ISO | PASS |
| Finder context Copy/Cut/Paste, Terminale cp/mv/appunti, lock e cold persistence | PASS |
| GPT Windows-like sana/discordante, inventario e rifiuto sicuro senza scritture | PASS |
| Installazione nativa 4Kn, cold boot, Recovery/reinstall senza alterare dati | PASS |
| Conversione/deattivazione cifratura in-place 4Kn, credenziali/file/snapshot | PASS |
| Avvio ottico desktop BIOS e UEFI su SATA | PASS |
| Mouse USB relativo in setup/desktop BIOS e UEFI, clic e trascinamento finestre | PASS QEMU |
| Riproduzione font, lock media live/installato/Recovery, bitmap panic nei 3 kernel | PASS |
| Contratto rollback, ABI/source-sync, cleanup e compilazione integrazione | PASS |

Le 8 suite sono: firma MBR, appunti rollback, inventario Windows sintetico,
media ottici, installazione 4Kn, conversione 4Kn, puntatore UEFI e puntatore BIOS.
Il trascinamento di finestre non è trasferimento drag-and-drop di file: quest'ultimo
è disabilitato e non viene annunciato come test passato.

12 suite host: partitions, windows-gaps, diskedit, installer-flow,
native4k-install, nvme, fs2-runtime, fs2-convert, input-maintenance,
finder-menus, finder-transfers e fs2-install. Queste ultime prove host verificano
fileops/appunti, non riabilitano il DnD del compositor. Non si riusano i conteggi
14 guest / 13 OS / 18 runtime della precedente 30B18.

## Tracciabilità e tentativi

755 input di produzione congelati prima dell'ultima ricostruzione e ricontrollati.
Una prima candidata 30B19 aveva già passato la nuova GUI; aggiunto un ulteriore
rifiuto per voci MBR residue senza tipo, poi ricostruita tutta la ISO e ripetuti
i gate finali. La prima campagna 4Kn è stata interrotta per questo rebuild dopo
installazione/cold boot, non conteggiata. Risultati preliminari distinti nell'evidenza.
Il batch host iniziale ha passato partitions/windows-gaps, poi si è fermato prima
del test diskedit per `mkfs.fat` non nel PATH; ripresa con `TOOLROOT=/` documentata.
Nessun assert di prodotto è stato rimosso per superare questi gate.

L'helper NVMe è conservato byte per byte dalla 30B18: non contiene il sistema
operativo. Il suo precedente test BIOS senza firmware NVMe resta storico; non
viene dichiarato un nuovo test di boot dell'helper in questa campagna.

## Limiti espliciti

**Il mouse sul PC fisico dell'utente non è certificato risolto.** Il rollback
richiesto è applicato e verificato in VM, non è un rollback dell'intero kernel.
La firma MBR assente è un caso riprodotto, non la diagnosi dimostrata del suo
disco. Prove Windows su fixture sintetiche, non sul suo disco né su Windows reale.
Fare un backup prima dell'installazione; GPT ambigue restano non installabili.
Nessuna visibilità EFI/Recovery cambiata; nessun Safari/WebKit completo incluso.
ISO non USB-DD ibrida. Provenienza/assenza di licenza negli ZIP font forniti
restano documentate in `licenses/FONTS.txt`.

## ISO finale

- File: `tinymacOS-10.2.5-x86_64.iso`
- Byte: 329506816
- SHA-256: `987a18ba4a8a1586791a855740b361475dbdc63fa12352a9631238c03e46056a`
- Font TFN2: `24add187754c57f6d8f160fe0a2608a9b946979713eec2b92dc653cb4daf97a6`

Evidenza: `workspace/local-10.2.5/hotfix/evidence-30B19.tar.xz`, ricevuta
`verification-30B19.json`. Pubblicazione/download controllati separatamente in
`publication-readback-30B19.json`. Le prove delle build precedenti restano storiche.
