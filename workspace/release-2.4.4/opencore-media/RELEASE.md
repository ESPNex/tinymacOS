# tinymacOS 2.4.4 Levante — OpenCore media revision 1

Aggiornamento richiesto della ISO 2.4.4: **avvio BIOS/OpenDuet/OpenCore realmente integrato nello stesso CD**, oltre all’avvio UEFI diretto precedente. Versione OS **2.4.4, build 21B5**, nome **Levante** invariato. Tag supporto: `v2.4.4-oc1`; la precedente release `v2.4.4` resta disponibile e immutata. La futura patch browser rimane 2.4.5 Levante.

## Download e verifica

- `tinymacOS-2.4.4-OpenCore-x86_64.iso` — **221.100.032 byte**
- `SHA256SUMS.txt`

```text
a4f6b48aebf492e243e9919818888ff4344f41a87b0a5ecd91c3cfda143fce34  tinymacOS-2.4.4-OpenCore-x86_64.iso
```

Release privata: https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.4-oc1 . Occorre accesso alla repository. Nessuna ISO o archivio di sorgenti è stato scritto nel workspace Arena.

## Catena di avvio effettiva

- **BIOS da CD/DVD:** El Torito → ISOLINUX/MEMDISK → volume MBR/FAT32 in RAM riservata → **OpenDuet 1.0.8 BlockIO → OpenCore 1.0.8 → loader tinymacOS → kernel/desktop nativi**.
- **UEFI:** immagine EFI El Torito originale e loader nativo diretto, byte-identici alla release 2.4.4. È disponibile anche il percorso esplicito `EFI/OC/OpenCore.efi`.
- Il BIOS non viene simulato copiando soltanto file EFI. Il test collega **esclusivamente questa ISO al lettore**, senza dischi/bootstrap HDD aggiuntivi e senza firmware OVMF nel profilo BIOS.
- La sola emulazione HDD El Torito è stata provata e rifiutata: SeaBIOS produceva `boot0af: error`. Per questo la release usa il trasporto MEMDISK, collaudato fino al desktop.

## Test della ISO allegata

PASS su **SeaBIOS** e **OVMF/UEFI**, con QEMU Q35, 1 GiB RAM, tastiere USB EHCI/xHCI e mouse USB nativo:

- setup account, autenticazione e desktop reali;
- `sw_vers`: 2.4.4 / 21B5;
- due CPU native online in entrambi i profili;
- stato e log verbose consultabili da Terminale;
- nel profilo BIOS verbose attivo da BOOTCFG; nel profilo UEFI diretto avvio normale;
- controllo QMP: nessun HDD o disco bootstrap separato collegato.

Audit: EFI.IMG, Install.esp e BaseSystem.dmg sono byte-identici alla 2.4.4 già collaudata per installer, Recovery, mouse/tablet e verbose persistente. Report: `workspace/release-2.4.4/opencore-media/`, harness `dualboot_qa.py`.

**SMP VM è implementato:** sulla base OS 2.4.4, rimasta identica, suite backend/runtime 15/15 PASS e stress concorrente con 4 e 8 vCPU PASS (quattro worker × 400 round, 1.600 syscall buffer in gara con mutazioni VM). Questo non equivale ancora a una libc pthread/TLS completa per WebKit.

## Licenze e sorgenti

OpenCore/OpenDuet: artefatti ufficiali 1.0.8 selezionati, lock e licenza in `tinymacOS/vendor/opencore-1.0.8/`. ISOLINUX/MEMDISK/ldlinux.c32 provengono dai pacchetti Debian SHA-256-pinned; copyright, GPL e accesso ai sorgenti sono inclusi anche nella ISO (`LICENSES/`).

**Sorgenti completi corrispondenti Syslinux**, estratti con le patch Debian applicate, nello stesso repository privato, branch `vendor/syslinux-6.04`, commit immutabile:

https://github.com/ESPNex/tinymacOS/tree/53791ac78b90525b81bd5b7ae692a681c4c3e12a

```sh
git clone --single-branch --branch vendor/syslinux-6.04 https://github.com/ESPNex/tinymacOS.git syslinux-source
```

Mantenere per ogni destinatario della ISO accesso equivalente al sorgente corrispondente, alle licenze e alle istruzioni di build. Nessun ZIP/TAR sorgente allegato: i file sono consultabili/clonabili estratti. Il branch separato conserva i circa 13 MiB upstream senza saturare lo snapshot da 128 MiB.

## Limiti importanti

- **L’installer produce ancora un sistema installato UEFI, non BIOS autonomo.** L’aggiunta BIOS riguarda l’avvio live della ISO da lettore CD/DVD.
- **Non è un’immagine USB ibrida DD.** La modalità DD e il boot USB legacy non sono dichiarati supportati. Per USB UEFI resta la modalità ISO con firmware UEFI, Secure Boot disattivato; nessuna prova fisica/Rufus eseguita.
- Il volume BIOS viene caricato in RAM: usare almeno **1 GiB consigliato**. Nessuna compatibilità universale con firmware, USB o controller fisici.
- La release è sperimentale, non per dati importanti e non aggiorna automaticamente un’installazione esistente. Usare dischi virtuali vuoti dedicati per provare l’installer.
- **Safari/WebKit non sono ancora inclusi o funzionanti.** Il prossimo lavoro è il runtime/port reale, poi UI classica, HTTPS/HTML/CSS/JavaScript e temi verificati prima della patch 2.4.5 Levante.
