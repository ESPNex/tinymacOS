# M2 — chiusura completa dei gate del macroblocco

**Esito: M2 completo rispetto ai criteri locali host/guest del piano 10.3. M1 preservato e ricollaudato sulla medesima ISO. Non è una release, né una certificazione hardware o Windows. M3 non è stato avviato da questo lavoro.**

ISO finale di sviluppo: SHA-256 `40f897e83a2e83bfb548cd63bac0be4f93586c54afded28261315a277d7a7c82`, 227639296 byte. Build: `/var/tmp/tmc-10.3/m2-final-build/m2.iso`, artefatto temporaneo NON pubblicato come release. `artifacts.json`, `source-manifest.json` e i log sono le ricevute persistenti; la permanenza di `/var/tmp` non è garantita.

## Implementazione conclusiva

- Capacità/LBA a 64 bit dall'ABI UI/kernel ai trasporti NVMe e AHCI, inventario GPT, Installer, manutenzione e adattatori FS2. Massimo supportato esplicito **16 TiB**, cioè 34359738368 settori da 512 B; oltre il limite si rifiuta, senza troncamento. AHCI ora scrive anche i byte LBA 32–47 del comando ATA.
- GPT calcolata nei settori logici nativi, coordinate interne in 512 B, PMBR con lunghezza saturata correttamente. Coesistenza su GPT 4Kn abilitata soltanto con immagini native disponibili e metadati validi. MBR non rappresenta extent oltre il proprio limite: attraversamenti dei 32 bit rifiutati prima delle scritture.
- Contenitore **v3** per gap oltre il campo v2 a 32 bit: span e posizioni Recovery/sorgente/DUET a 64 bit, CRC sull'intero descrittore; dimensione 512 B e prefisso/posizione CRC v1/v2 invariati. Lettura v1/v2 conservata, writer v2 per span piccoli. FS2 riceve l'intero spazio dati del gap selezionato, non le partizioni estranee né l'intero disco.
- Boot UEFI/Recovery legge blocchi firmware 512/4096 e segue FAT12/FAT16 secondo geometria reale. Il prefisso ESP nativo 4Kn può essere FAT12: non va forzato a FAT16. 4Kn collaudato in **UEFI**, non BIOS/4Kn.
- Unità SI e input MB decimali coerenti con i nuovi limiti, round-trip esatto del massimo `17592186.044416` MB, allineamento per difetto e rifiuto di overflow/precisione eccessiva.
- Preservate le correzioni già presenti: home con nome account senza rinominare percorsi persistenti, unica azione «Nuovo snapshot», geometrie distinte disco/contenitore/dati e avvisi di sicurezza.
- Editor e filesystem legacy non diventano magicamente FS2: operazioni legacy con extent oltre UINT32 e operazioni 4Kn non supportate vengono rifiutate senza scrivere/troncare. Questo limite è distinto dall'installazione e I/O FS2 >2 TB effettivamente collaudati.

## Matrice di accettazione finale

| Criterio del piano | Prova | Esito |
|---|---|---|
| 999/1000, 10^6/10^9/10^12, overflow, buffer | Formatter reale e parser MB; massimo 16 TiB esatto e arrotondamento conservativo | PASS normale + ASan/UBSan |
| Compatibilità contenitori | v1 fisso, v2 fino a 1 TiB nel test, v3 fino a 16 TiB, bounds malformati; offset CRC verificato staticamente | PASS normale + ASan/UBSan |
| Geometria UI, home e snapshot | Vera paint Utility Disco, foundation e screenshot del guest con `CapacityQA`; nessuna duplicazione azione | PASS host/guest |
| GPT grande e coordinate elevate | Vero Installer/manutenzione a 16 TiB con extent presso il fondo; foreign entry/canary, backup GPT danneggiato e limite superiore | PASS normale + ASan/UBSan |
| FS2 a indirizzi elevati, non solo capacità visualizzata | Format/import/verify reale con base >2 TiB; write/read 16 KiB con offset relativo in settori >UINT32 | PASS normale + ASan/UBSan |
| 4Kn GPT, FS2 e cifratura | Installazione nativa, conversione Recovery encrypt/decrypt/ripresa; file/account/snapshot e byte pubblici preservati | PASS normale + ASan/UBSan |
| NVMe 3 TiB / 4096 B | Vera GUI installa nel gap da 2,5 TiB; cold boot nativo e cold Recovery senza ISO | PASS guest completo |
| NVMe 3 TiB / 512 B | Stessa sequenza, esecuzione indipendente | PASS guest completo |
| AHCI/SATA 3 TiB / 512 B | Stessa sequenza, trasporto AHCI reale del guest | PASS guest completo |
| Tre viste, stessa installazione | Terminale/Utility/Installer prima della scrittura e dopo cold boot; nuova lettura in Recovery sul medesimo disco | PASS in tutti e tre i guest |
| Partizioni estranee | Tutti i byte delle due partizioni sintetiche verificati, più voci GPT preservate; separate regressioni FAT32/NTFS/RAW reali e protezioni Windows-gap | PASS guest + host normale/sanitizzato |
| Regressioni storage | Partizioni/EBR/PMBR, diskedit + fsck.fat/readback indipendente, resize nativo, FS2 install/runtime/import/conversione, NVMe transport | PASS normale + ASan/UBSan |
| Regressione M1 sulla ISO M2 finale | Intera matrice USB/pixel; UTF-8 144000/141735 B salvati/riaperti SHA-256 esatto, Recovery; vero FV003 GUI, password errata/corretta senza eco | PASS guest |
| M0/font/USB/overlay/build | Foundation finale, M0 workspace, 568 input protetti, TFN/panic font ricostruiti dai font forniti, build BIOS/UEFI/Recovery | PASS |

Il log `host.log` è una sola esecuzione completa con `set -e`, conclusa con `PASS M2 host gates`. Tutti i gate sopra hanno esito effettivo prima del commit: nessuna suite ancora in background e nessun PASS ricavato da un semplice avvio del processo.

## Geometrie indipendenti dei guest

Per ogni scenario: disco `6442450944 × 512 = 3298534883328 B` (3 TiB), gap selezionato `5368709120 × 512 = 2748779069440 B` (2,5 TiB), dati FS2 `5368586240 × 512 = 2748716154880 B`. Letture indipendenti dell'host verificano extent GPT, versione/CRC v3 e conteggio blocchi FS2; il guest usa il vero percorso Installer/boot/filesystem.

Le interfacce mostrano **3.2 TB** per il disco e **2.7 TB** per il contenitore in SI, non conversioni binarie rinominate. Il volume dati è distinto. I piccoli gap di allineamento da 1.0 MB fuori dal contenitore rimangono reali: non sono stati assorbiti per nasconderli. `Documents/m2-proof.txt` e l'account importati dalla live sono usabili al primo cold boot nativo.

Le partizioni estranee di questi tre dischi sono **sintetiche basic-data**, con zeri e canary: non contengono un Windows funzionante. `m2_sparse_check.py` sfrutta SEEK_DATA/HOLE del filesystem host per certificare gli zeri non allocati e controlla ogni settore allocato delle partizioni esterne, non solo quattro campioni. Tutti i 32 settori non nulli attesi coincidono; voci GPT esterne e canary rimangono invariati dopo installazione e cold boot. Le regressioni host con FAT32/NTFS/RAW reali sono distinte e non vengono spacciate per certificazione Windows fisica.

## Evidenze, provenienza e tentativi preliminari

- `nvme-4096-result.json`, `nvme-512-result.json`, `sata-512-result.json`: tre esecuzioni fresche complete sulla **stessa ISO finale**, `passed: true`, `release_ready: false`, `physical_hardware_tested: false`.
- Screenshot 4Kn delle fasi live/installazione/cold/Recovery; Terminale Recovery anche per gli altri due bus/formati. Alcune immagini sono **ritagli lossless della finestra**, non schermate ricostruite: rettangoli e hash del PNG originale in `screenshots.json`. Escluso soltanto lo sfondo per il budget del workspace. Le altre immagini sono complete.
- Log seriali dei tre boot per scenario (CRLF normalizzato a LF e whitespace finale rimosso; hash originali/normalizzati in `serial-log-normalization.json`), log test/build/foundation/M0/font e regressioni M1; `m1-acceptance-result.json` e `m1-terminal-vault-result.json` sono le nuove ricevute M2, non quelle storiche M1.
- Primo tentativo 4Kn: l'Installer ammetteva soltanto dischi vuoti; corretto il gate GPT, poi risolto il traversal FAT12 per il cold Recovery. Questi tentativi non sono accettazione. La prima catena host aveva `mkfs.fat` fuori dal PATH: la suite finale completa usa `/usr/sbin` ed è passata. Successivamente aggiunto rifiuto esplicito dell'extent MBR oltre 32 bit, ricostruita la ISO e ripetuti **tutti e tre** i guest e M1 sulla build finale.
- Dopo questa build sono cambiate soltanto fixture/script host e documentazione, non i sorgenti funzionali del sistema. Il test FS2 aggiuntivo ad alto offset ha passato entrambe le varianti nella suite finale.

## Riproduzione

Richiede toolchain/dependency del progetto, OVMF/QEMU, OCR, strumenti FAT/NTFS e spazio temporaneo; mai puntare a dischi utente/fisici.

```sh
export PATH=/usr/sbin:$PATH
export TMC_BUILD=/var/tmp/tmc-m2-build TMC_ISO=m2.iso
bash tinymacOS/build.sh
bash tinymacOS/tests/test-m2-acceptance.sh
bash tinymacOS/tests/test-10.3-foundation.sh
python3 tinymacOS/tests/test-m0.py --workspace
python3 tinymacOS/tests/test-font-no-fallback.py
export TMC_STAGE_ISO=$TMC_BUILD/m2.iso
for spec in nvme:4096 nvme:512 sata:512; do
  TMC_M2_STORAGE=${spec%:*} TMC_M2_LOGICAL=${spec#*:} \
    TMC_QA_DIR=/var/tmp/m2-qa-${spec/:/-} \
    python3 -u tinymacOS/tests/m2_capacity_guest.py
done
export TMC_QA_DIR=/var/tmp/m2-m1-regression
python3 -u tinymacOS/tests/m1_acceptance_guest.py
python3 -u tinymacOS/tests/m1_terminal_vault_guest.py
```

## Limiti che restano separati

Nessuna prova hardware fisica, BIOS/4Kn, Windows installato reale o riempimento fisico di 16 TiB; il massimo ha copertura host sparsa ad alti indirizzi, i guest arrivano a 3 TiB. Nessuna nuova modalità GOP, accelerazione NVIDIA, esportazione exFAT, installazione OpenCore principale o release/tag 10.3. M3–M7 restano da implementare/verificare nel loro ordine. Safari, font forniti, percorso USB canonico, DnD e numeri pubblici di versione preservati.
