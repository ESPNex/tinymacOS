# Validazione revisione 10.2.3 CoW

ISO dual-boot BIOS/UEFI, 10.2.3 / 30B14:

```text
SHA-256 71d94c029deb8fa820d5f71dca8633d0ddcc85b784e55485a60343a7928d4dc0
```

## PASS

- Build desktop e kernel Recovery; ispezione indipendente di ELF, sigilli dei payload, font e plist su tutti i volumi. Filtri OpenCore presenti anche nell’ESP di installazione.
- Installazione reale GUI, boot BIOS HDD-only, riavvio, UEFI sullo stesso disco; Recovery senza ISO, backup/ripristino/reinstallazione e credenziali/documento conservati. Nessun intervento del test per sbloccare il picker OpenCore.
- CoW host normale + ASan/UBSan: condivisione iniziale, un vecchio blocco condiviso, versioni distinte, riapertura, reclamation, pool pieno, journal e settori strappati, migrazione legacy sovrapposta.
- Driver reale e FileVault reale: mappe/pool attraverso il dispositivo autenticato, lock/reboot/unlock, ciphertext alterato rifiutato. Resize nativo 32→24→32 MiB con snapshot/backup/file conservati; capacità realmente diversa, 382 contro 286 nuovi file da 8 KiB.
- CoW guest: A/B conservati mentre il file attivo è C; lettura indipendente di superblocco/mappe/checksum e **2 settori conservati anziché 12.576** di due cloni. Riavvio BIOS, rinomina/eliminazione, ripristino di uno snapshot mantenendo l’altro e backup/ripristino Recovery.
- FileVault da Impostazioni, password errata rifiutata, interruzione prima della conversione e ripresa al boot BIOS; scansione offline senza nome/contenuto del documento in chiaro. Ripetuto sul disco misto MBR con base nativa rilocata e hash/file FAT32/NTFS/RAW preservati.
- Editor FAT32, installer GPT/MBR, variabili EFI, NTFS nativo, crittografia, materiali e stack USB/input: suite host normali/sanitizzate, dove previsto, con lettori indipendenti e guasti iniettati.
- Guest NTFS: GPT 8→6 MiB e MBR 8→9 MiB, file letto con ntfscat, controllo indipendente, FAT32/RAW invariati e nuovo resize dirty rifiutato. Guest editor: FAT32, nativo 32→24→32 MiB, pannelli/conferme, disco montato protetto e BootOrder OVMF riletto.
- Dual boot GPT/MBR: partizioni esterne byte-identiche, letture mtype/ntfscat, avvii BIOS/UEFI/Recovery e precedente MBR sentinel. Finder doppio clic, menu e Recovery scura/opaca invariati.

Il manifesto `VALIDAZIONE.json` distribuito con la release contiene le ricevute guest e gli hash dei log host. Gli errori dei tentativi diagnostici non vengono riclassificati come PASS né attribuiti alla nuova ISO.

**Non certifica Windows o hardware fisico.** Il sentinel MBR non è Windows; nessun test sostituisce un backup esterno verificato. Migrazione legacy, resize e modifiche alle tabelle non hanno rollback universale contro interruzioni.
