# Regressione finale — tinymacOS 10.2.4 / 30B15

**Esito: PASS, 34 gruppi di verifica** (18 host e 16 media/guest). Tutti i risultati media/guest riportati sono associati esattamente alla seguente ISO BIOS/UEFI, non a prototipi precedenti.

SHA-256 ISO:
```
7b6059a4c82fee9add7a642690ac679d554a287a9c7ed277cff2279017750b21
```

## Risultati principali

- Desktop live: **Installa tinymacOS Tramontana**. Recovery: **Reinstalla tinymacOS Tramontana**. Eseguibile nativo identico byte per byte; nome di finestra, barra, scelta Recovery, icona desktop e metadati contestuali verificati.
- Installazione grafica BIOS da zero; avvii BIOS/UEFI dal solo disco, documenti persistenti, Recovery interna senza ISO e nuovo wizard di reinstallazione con conferma obbligatoria.
- Reinstallazione: l'intera partizione dati è rimasta identica (SHA-256 `ed050f57bd99546c3b05cde88034eb603f5e13a06b5430581199724177e7b46b`); il sistema riscritto corrisponde esattamente alla sorgente locale sigillata.
- Nessun residuo della barra nel primo frame Recovery e nei due secondi successivi: sei combinazioni BIOS/UEFI × 1/2/4 CPU. Prova host di scritture splash tardive/concorrenziali con normale e ASan/UBSan.
- Dischi commerciali da 1 TB e limite 1 TiB: installer/manutenzione effettivi su GPT/MBR ad alto LBA, normale e ASan/UBSan; rifiuto di GPT corrotta/capacità eccessiva prima delle scritture.
- Disco GPT sparso **1.099.511.627.776 byte** in QEMU/AHCI: installazione grafica a LBA **2.146.959.360**, avvio UEFI e BIOS/OpenDuet senza ISO, documento persistente e Recovery interna. CRC delle due GPT, voce straniera e sentinelle esterne conservati.
- CoW reale: due snapshot storici mentre il file live cambia; lettura indipendente delle mappe: **2 settori storici unici**, anziché **12576** settori di due cloni completi. Rename/delete/restore, riavvio BIOS e backup/ripristino Recovery conservano le viste storiche.
- FileVault da Impostazioni e su installazione rilocata in disco misto: password errata rifiutata, interruzione prima della conversione, ripresa al boot BIOS, documento conservato e nome/contenuto non presenti in chiaro sul disco. Prove host anche di cifratura/autenticazione CoW e manomissioni.
- Resize FAT32/NTFS/nativo, creazione/eliminazione partizioni, protezione del disco montato, controlli indipendenti fsck/mtype/ntfscat, fallimenti di scrittura e dati preesistenti conservati.
- Coabitazione GPT/MBR: avvii BIOS/UEFI/Recovery, BootOrder OpenCore e backup dell'ordine precedente, voci e hash di FAT32/NTFS/RAW conservati; scelta BIOS del sentinel MBR originale.
- Input USB, movimento Recovery continuativo, menu, Finder a doppio clic, menu bar e materiale opaco/glass: PASS. Font accettato invariato.

## Matrice completa

| Gruppo | Esito |
|---|---|
| `media` | PASS |
| `install` | PASS |
| `cow-guest` | PASS |
| `vault-settings-bios` | PASS |
| `recovery-bios` | PASS |
| `recovery-uefi` | PASS |
| `recovery-menu` | PASS |
| `ntfs-native-guest` | PASS |
| `diskedit-guest` | PASS |
| `finder-double` | PASS |
| `menubar-clear` | PASS |
| `dualboot-gpt` | PASS |
| `dualboot-mbr` | PASS |
| `vault-mixed` | PASS |
| `recovery-splash` | PASS |
| `large-disk-guest` | PASS |
| `host-native-cow` | PASS |
| `host-native-resize` | PASS |
| `host-boot-default` | PASS |
| `host-boot-display` | PASS |
| `host-installer-flow` | PASS |
| `host-diskedit` | PASS |
| `host-partitions` | PASS |
| `host-ntfs` | PASS |
| `host-ntfs-sanitized` | PASS |
| `host-vault` | PASS |
| `host-material` | PASS |
| `host-input-maintenance` | PASS |
| `host-legacy-input` | PASS |
| `host-linux-usbcore-enumeration` | PASS |
| `host-linux-usbcore` | PASS |
| `host-linux-usbhid` | PASS |
| `host-linux-xhci` | PASS |
| `large-disks` | PASS |

## Note sul banco di prova

Due tentativi diagnostici non sono conteggiati come accettazione: una creazione nativa ancora in esecuzione ha superato l'attesa generica di 24 secondi; le scritture pesanti ora hanno un limite esplicito di 180 secondi. Un tentativo di avvio misto ha inviato `bootverbose` prima della fine del precedente `echo`, perdendo la prima lettera. Il driver di test ora attende il vero footer del Terminale «Sessione locale», con il suo font, prima di inviare un altro comando. Le asserzioni di contenuto, i controlli grezzi e le verifiche indipendenti non sono stati rimossi; entrambi i percorsi sono stati rieseguiti da fixture nuove e sono passati. Le prove diagnostiche restano separate dai risultati accettati.

## Confini della validazione

- QEMU/TCG, OVMF e BIOS/OpenDuet: **non** certificazione di hardware fisico o di Windows reale. Il precedente sistema è un sentinel sintetico dichiarato.
- 1 TiB è il limite del **disco SATA/AHCI da 512 B per settore**, non del volume dati nativo: quest'ultimo resta **8–32 MiB**, 64–384 slot, 8 KiB/file e 8 snapshot.
- Il test sparso da 1 TiB controlla realmente alto LBA, installer, boot e GPT; non esegue una scansione completa di 1 TiB vuoto né certifica un filesystem NTFS/FAT32 da 1 TiB.
- Reinstallazione da copia locale verificata, non aggiornamento online. Scritture delle tabelle/reinstallazione non atomiche in caso di perdita di alimentazione; resize cifrato non supportato.
- Dopo resize NTFS è richiesto il controllo da Windows. Nessun audit crittografico esterno.
- Backup esterno verificato obbligatorio prima delle operazioni sui dischi.

L'asset `VALIDAZIONE.json` associa commit, ISO, hash dei log host e risultati completi dei guest. `SHA256SUMS.txt` verifica ISO, sorgenti e rapporto. Le release precedenti non devono essere sostituite.
