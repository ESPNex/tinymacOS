# tinymacOS 2.3.0 Sirocco

**Build 21A1 · ISO UEFI x86-64 · supporto unico con BaseSystem integrato**

**[Scarica la ISO da Google Drive — 51,71 MiB](https://drive.google.com/uc?id=15Rws_zhtej2AkWuIMq_hMGZqiP_rBsov&export=download)** · [Checksum](tinymacOS-2.3.0-SHA256.txt)

Serve **solo questo file**: niente secondo download, niente immagine USB separata. Le release precedenti su Drive (2.2.0, 2.1.0, 2.0.2, 2.0.1, 2.0 e 1.3) sono rimaste invariate.

## Il cambiamento di questa versione

La 2.3.0 «Sirocco» (build **21A1**) è la versione **sistema** di questa serie: kext, rete e protezione del sistema, con Impostazioni finalmente completa.

1. **Impostazioni completa, scuro ovunque.** La modalità *Scuro* ricolora **finestre, barre, menu bar e pulsanti** — non più solo lo sfondo e il Dock: in una sessione scura Finder e Impostazioni risultano entrambe scure, con la chrome interna coerente. **Ogni colore di accento ha la sua variante chiara e scura**: lo stesso accento (Blu, Azzurro, Sirocco, …) si applica correttamente in entrambi i temi e tinge anche i pulsanti e la selezione di Finder. Il tema e l'accento restano separati dagli sfondi, come nella 20C2.
2. **Sistema kext.** Il kernel carica estensioni da `/System/Library/Extensions` (bundle con `Info.plist` binario ed eseguibile **ELF64 ET_REL**, posizionato in memoria eseguibile prima del salto). Entrano i due driver di rete: `e1000.kext` e `wifi.kext`. Comandi dedicati nella shell: `kextstat`, `kextload /percorso/Bundle.kext`, `kextunload bundle-id`.
3. **Protezione dell'infrastruttura di sistema (SIP, alla macOS).** `csrutil status` mostra lo stato; `csrutil enable|disable` **rifiutano fuori dalla Shell di Recupero** con il rimando a premere R durante la mela. La protezione è applicata ai percorsi di sistema (`/System`, `/usr`, `/bin`, `/sbin`) e lo stato persiste nella variabile EFI `tinymacOS-csr` scritta dalla Recupero; in assenza di variabile vale il default **attiva**.
4. **Rete vera, con lwIP.** Il kernel integra **lwIP 2.2.1** e i driver stanno nei kext:
   - **Ethernet e1000** (la scheda virtuale di QEMU): con `-netdev user -device e1000` il guest fa **DHCP** (10.0.2.15), vede il **MAC** (`ifconfig en0`) e fa **ping** reale del gateway (`ping 10.0.2.2`);
   - **Wi‑Fi simulato** (nessun hardware Wi‑Fi in QEMU: etichettato onestamente *radio simulata*): `wifi scan` elenca le reti dell'aria simulata (SiroccoNet, …), `wifi connect <ssid>` associa, `wifi status` mostra stato e disclaimer, `ifconfig wl0` mostra l'interfaccia;
   - `ifconfig` e `ping` funzionano su entrambe.
5. **DPI costante e leggibile.** A qualunque risoluzione, fino a **1920×1080**, la taglia logica dell'UI resta la stessa: a Full HD niente diventa piccolo.
6. **Identità Sirocco.** Terminale, `sw_vers` e tutta la documentazione dicono **Sirocco** (non più Riviera): `sw_vers` riporta `2.3.0` / build `21A1`.

Restano dalla 20C2 le *Impostazioni* alla System Settings di Apple, **Mission Control** (F10), le 19 risoluzioni fino a 1920×1080 e il ripristino dell'account inaccessibile in due passi dal login.

## Il supporto unico (invariato)

Un solo file: `BaseSystem.dmg` è **dentro la ISO**, in `/Recovery/`, copiato identico byte per byte nella partizione Recovery dopo l'installazione (`diskutil info /` nella shell di Recupero mostra l'origine e i byte verificati). Si avvia anche senza CD e senza rete. Il puntatore predefinito resta il `usb-tablet` QEMU.

## Avvio in QEMU

Scegli **Q35 x86-64, UEFI/OVMF senza Secure Boot, 512 MiB di RAM**, disco SATA vuoto dedicato, CD con la ISO e unico puntatore `usb-tablet` accanto alla tastiera USB:

```sh
-device qemu-xhci,id=xhci \
-device usb-kbd,bus=xhci.0,port=1 \
-device usb-tablet,bus=xhci.0,port=2
```

`./run.sh` del progetto configura già questo profilo; `TMC_POINTER=mouse` sceglie il mouse relativo. Durante la mela premi più volte **R** (o F12) per il Recupero. Dopo l'installazione: spegni, rimuovi la ISO, riavvia col solo disco (`BOOT_DISK=1 ./run.sh`) e premi di nuovo R.

**Per la rete** aggiungi la scheda virtuale (o `TMC_NET=1 ./run.sh`):

```sh
-netdev user,id=net0 -device e1000,netdev=net0
```

## Verifiche eseguite su questa build

- `tests/cli_matrix.py` — **23/23**: diskutil apfs (lista, snapshot, crittografia, lock/unlock), fsck/mount/newfs dedicati, Time Machine, Utility Disco, **sw_vers, csrutil, kextstat, kextload/kextunload, wifi, ifconfig, ping**, spegnimento pulito.
- `tests/settings_gui.py` — **7/7**: pannelli Aspetto/Schermo/Sicurezza/Backup/Data e ora/Generali e ricerca laterale.
- Ciclo completo delle **19 risoluzioni** (fino a 1920×1080), CapsLock e Mission Control — OK.
- `netboot.py` — **16/16** con `-netdev user -device e1000`: serial lwIP/kext, kextstat, MAC, **DHCP 10.0.2.15**, **ping gateway con risposte**, wifi scan/connect/status, **ping dell'AP simulato**, csrutil ×2, sw_vers.
- Suite di accettazione (`tests/integration.py`): **50 controlli OK** tra riviera_live, riviera_guard, riviera_contained, riviera_pointer, riviera_disk, riviera_apps, riviera_integrity, riviera_runtime, crypto e il test HID tablet (160 combinazioni).

## Onestà

- Il **Wi‑Fi è simulato** (QEMU non ha hardware Wi‑Fi): l'interfaccia e i comandi funzionano sull'aria simulata, con disclaimer esplicito a schermo. L'**e1000** è la scheda virtuale di QEMU: DHCP e ping reali passano da lwIP fino a slirp.
- **Nessun codice Apple, nessuna firma Apple.** «SIP» è un'implementazione **ispirata a** macOS (comandi e comportamento analoghi), non Apple. I kext sono nel nostro formato ELF64 ET_REL, non bundle macOS firmati.
- Il sistema non si aggiorna da solo: per provare la nuova versione serve la ISO nuova o una installazione dedicata.

## Verifica del file

```
SHA-256 — tinymacOS 2.3.0 Sirocco (21A1)

7c0e6ee8dbc4cf37134aaf216edc702e3d0c42fa157c9f1fa7ccc52011d35ac3  tinymacOS-2.3.0-x86_64.iso  (54218752 byte)

20c891a282e7d02d7082b738ae8ad16d062ceb64038ea1e8b9bfcf65b9c1a7d7  /Recovery/BaseSystem.dmg (3147889 byte, dentro la ISO)
```
