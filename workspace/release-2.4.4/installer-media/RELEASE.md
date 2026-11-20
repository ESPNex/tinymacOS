# tinymacOS 2.4.4 Levante (21B5) — manutenzione OpenCore installer OC2

Aggiornamento della manutenzione richiesto: **anche il disco installato avvia autonomamente in BIOS attraverso OpenDuet/OpenCore**, senza tenere la live nel lettore. Tag `v2.4.4-oc2`; le precedenti OC1 e 2.4.4 restano conservate. OS e stringhe attive restano **2.4.4 / 21B5 / Levante**; futura patch browser **2.4.5 Levante**, non inclusa qui.

Release privata: https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.4-oc2

## Download

`tinymacOS-2.4.4-OpenCore-Installer-x86_64.iso` — **221.485.056 byte**.

```text
714799fb0af8f6c88a051c7fba89f02a5cf68557d9a7e458cea112907726ec22  tinymacOS-2.4.4-OpenCore-Installer-x86_64.iso
```

## Avvio e installazione

- CD BIOS: El Torito → ISOLINUX/MEMDISK → OpenDuet **BlockIO** 1.0.8 → OpenCore 1.0.8 → loader/kernel nativi.
- CD UEFI: immagine EFI El Torito e loader nativo diretto, aggiornati con i nuovi payload installatore/Recupero.
- Disco installato BIOS: ponte MBR di 440 byte scritto dall’installer → OpenDuet **completo USB/AHCI** 1.0.8 a LBA 64 → `EFI/OC/OpenCore.efi` → `EFI/TMC/BOOTX64.EFI` → kernel/desktop nativi.
- Stesso disco installato UEFI: launcher OpenCore ufficiale `EFI/BOOT/BOOTX64.EFI` → OpenCore → loader/kernel nativi. Non dipende da una voce NVRAM precedente.
- GPT con MBR protettivo, ESP FAT16 24 MiB, dati, Recovery e sorgente sigillata mantengono gli stessi offset. Non si forza FAT32 in una ESP troppo piccola. Il ponte carica il file OpenDuet contiguo nel gap riservato prima dell’ESP; non usa `boot1f32` per il disco installato.
- L’installer verifica l’intero disco vuoto prima di scrivere; scrive e rilegge OpenDuet **prima** di attivare GPT/MBR. Non è una migrazione automatica delle installazioni vecchie e non modifica dischi preesistenti per aggiungere il BIOS.
- Configurazione installata senza `-v` forzato. Il comando `bootverbose` continua a controllare la preferenza su disco.

## QA effettiva sulla ISO con il checksum sopra

QEMU Q35, SeaBIOS oppure OVMF, 1 GiB, 2 vCPU; nessuna prova fisica implicita.

| Prova | Esito |
|---|---|
| La stessa ISO, solo lettore CD, BIOS e UEFI: setup, login, desktop, versione e 2 CPU | PASS |
| Live BIOS → vero installer grafico su disco SATA virtuale vuoto | PASS |
| Spegnimento, rimozione completa del dispositivo CD, avvio BIOS dal solo disco | PASS |
| Login, documento persistente, spegnimento/nuovo processo QEMU, secondo login BIOS | PASS |
| Verbose abilitato dal Terminale, osservato al riavvio e poi disabilitato | PASS |
| Stesso disco avviato in UEFI con stato OVMF nuovo | PASS |
| KernelCache alterato su copia sacrificabile → rifiuto del digest → Recupero locale | PASS |
| Reinstallazione offline e ritorno al desktop BIOS; documenti invariati | PASS |
| Recupero BIOS con mouse USB normale e con tablet QEMU | PASS |
| Recupero UEFI con mouse USB normale | PASS |
| ESP dopo avvii/ripristino identica byte per byte alla sorgente sigillata; Recovery corrisponde alla ISO | PASS |
| Byte estraneo a LBA 64: rifiuto dell’installer, SHA-256 dell’intero disco identico prima/dopo | PASS |
| Audit indipendente GPT/FAT, manifest SHA, bundle, Mach-O/ELF e BaseSystem UDIF | PASS |

Report JSON e due screenshot reali sono in questa directory. Script ripetibili in `../bios_installer_qa.py`, `../bios_installer_guard_qa.py`, `../bios_recovery_qa.py`, `../dualboot_qa.py`; tutti usano solo nuovi file disco esterni sotto `/var/tmp`.

## Correzioni che rendono il test completo

- La variante BlockIO di OpenDuet resta necessaria per il disco MEMDISK ottico, ma non fornisce da sola lo stack USB richiesto da Recupero: l’installato usa la variante completa, più XhciDxe/OpenUsbKbDxe ufficiali e il percorso pointer USB_IO già presente in Recupero.
- Recupero enumera i SimplePointer invece di fidarsi soltanto del primo protocollo, che può essere il PS/2 inattivo. Il test mouse tiene conto del percorso firmware più lento, senza presumere la latenza del driver kernel.
- OpenCore apre la directory `NVRAM` con CREATE anche quando lo schema legacy è vuoto: la directory vuota viene ora creata **prima del sigillo**, evitando modifiche FAT al primo avvio BIOS. Nessun file NVRAM variabile viene escluso dal digest del sistema.
- Driver e launcher aggiunti provengono dallo stesso archivio OpenCore 1.0.8 già verificato; hash in `tinymacOS/vendor/opencore-1.0.8/vendor-lock.json`. Licenze redistribuite nei supporti.

## Limiti e seguito browser

- Questa revisione **non contiene Safari/WebKit, JavaScriptCore, WebCore o HTTPS funzionante**. Include il nuovo ABI TLS per-thread FS, già verificato separatamente con 1/4/8 vCPU: è un prerequisito runtime, non il protocollo TLS/HTTPS e non pthread completo.
- Nessuna promessa di BIOS/hardware/USB universale. Verificati i profili virtuali indicati; il kernel non acquisisce automaticamente tutti i controller USB supportati dal firmware OpenDuet.
- ISO ottica, **non immagine USB DD ibrida**. Nessuna validazione fisica o Windows/Rufus. Non usare strumenti QA su dischi reali.
- Sorgenti scritti nel workspace e sincronizzati nella repository privata prima delle build; ISO e scratch esclusivamente esterni. I commit GitHub verificano il backup remoto, non provano la persistenza di uno snapshot Arena.

## Ricostruzione

Dal checkout del tag OC2 e dopo il provisioning documentato della toolchain:

```sh
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc-bios-install-system bash tinymacOS/build.sh
python3 tinymacOS/tools/dualboot-iso.py \
 /var/tmp/tmc-bios-install-system/tinymacOS-2.4.4-x86_64.iso \
 /var/tmp/tmc-oc2-stage/tinymacOS-2.4.4-OpenCore-Installer-x86_64.iso
```

L’output del secondo comando deve essere un nuovo percorso. Il ponte MBR si assembla con GNU as/ld; nessun NASM o download implicito. Il vendor selezionato è estratto e verificabile, non un archivio sorgenti consegnato al posto dei file.
