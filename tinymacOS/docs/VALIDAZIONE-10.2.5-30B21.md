# tinymacOS 10.2.5 — validazione 30B21

Ripristinato il profilo di compilazione USB/HID della ISO ufficiale 10.2.4.
La 30B20 usava invece la facade nativa: il rollback dei sorgenti da solo non
ripristinava il percorso avviato. 30B18/19/20 sono fallite sul PC dell'utente.
Il cambio di profilo è provato sui binari distribuiti; la causa fisica e la
riuscita sul PC interessato non sono ancora dimostrate.

## Controlli freschi sulla stessa ISO finale

- 8 profili guest canonical: BIOS e UEFI, due mouse xHCI + tastiera USB,
  EHCI, hub xHCI, tablet xHCI. PS/2 disattivata. usbhid registrato, facade
  nativa inattiva, worker in attività, setup e Terminale con tastiera USB,
  pixel del cursore corretti, teardown e riconnessione della periferica.
- Recovery ottica senza tinymacOS: mouse relativo, clic, guida alla nuova
  installazione nel live; metadati e bordi occupati della fixture invariati.
- GPT/PMBR: consenso e annullamento, sola riparazione 55AA se autorizzata,
  installazione nel vero spazio non allocato, backup MBR e cold boot NVMe.
- Finder Taglia/Copia/Incolla e appunti Terminale, persistenza a freddo.
- Avvio ottico SATA BIOS e UEFI, account, desktop, Terminale e documenti.
- Installazione effettiva NVMe 4Kn da GUI, cold boot, Recovery/reinstall,
  account/documenti/snapshot preservati; conversione e disattivazione
  cifratura in-place 4Kn, verifiche dati e partizioni pubbliche invariate.
- 9 suite host: ABI canonical ASan/UBSan; USB retry, input maintenance,
  xHCI, usbhid, installer flow, Windows gaps, native4k install, FS2 install.
- Controllo codice macchina live e Recovery: startup canonical, worker nello
  scheduler, HID completo collegato. Lo stesso controllo rifiuta la 30B20.
- Audit estratto dalla ISO: kernel live/installato e Recovery identici ai
  binari controllati; font UI/Mono bloccati, panic e versione 30B21 corretti.

Totale: 9 suite host e 14 suite guest. Sono verifiche software/QEMU,
**non una certificazione del mouse fisico o di Windows reale**.

## Conservazione e limiti

Una sola ISO BIOS+UEFI, stessa release 10.2.5. Desktop/input della 10.2.4,
DnD file disabilitato; FS2/4Kn/cifratura e appunti preservati. Font forniti,
nessun fallback DejaVu. Nessuna modifica visibilità EFI/Recovery. RAW/MSR/NTFS
mai considerate spazio libero; protezioni GPT non bypassate. Recovery
Reinstalla senza sistema preesistente non è l'installer di una nuova copia:
la guida indica il desktop live, senza presumere una tabella Windows corrotta.
Helper NVMe invariato dalla 30B18: non è una nuova prova del suo boot.
Safari/WebKit completo non incluso. ISO non USB-DD ibrida. Backup necessario.

- ISO: `tinymacOS-10.2.5-x86_64.iso`
- Byte: 329707520
- SHA-256: `b1f19e5222f293ceec14fac26a386acc41eb1670d8d9c46504cdf1ea6b12d682`
- Note: `tinymacOS/docs/NOTE-10.2.5-30B21.md`
- Evidenze: `verification-30B21.json`, `evidence-30B21.tar.xz`
- Pubblicazione: `publication-readback-30B21.json`, da verificare separatamente.

## Conservazione delle evidenze storiche

L’archivio locale duplicato è stato rimosso durante la pulizia M0 della 10.3, solo dopo riscaricamento e verifica SHA-256. Rimane disponibile al [commit immutabile pre-pulizia](https://raw.githubusercontent.com/ESPNex/tinymacOS/3453dc8a874be85197d7118df996d31788deb23d/workspace/local-10.2.5/hotfix/evidence-30B21.tar.xz). Hash, dimensione e comando di ripristino sono registrati in `workspace/local-10.3/m0/cleanup.json`. Nessun asset della release pubblicata è stato modificato.
