# tinymacOS 10.2.5 — build 30B19

Aggiornamento della stessa release 10.2.5, non una 10.3. Una sola ISO BIOS+UEFI.

## Rollback mouse richiesto

`desktop/main.c` e `drivers/input.c` sono tornati **byte per byte** alla 10.2.4
(commit `1a4ca9af4c4e7bb42cc819e7f931da34f6a4af69`). Eliminato il relativo
header per il budget eventi, ora inutilizzato. Il trasferimento di file tramite
drag-and-drop nel desktop è disabilitato. Restano Taglia/Copia/Incolla del
Finder e i comandi/appunti del Terminale. Nessun downgrade dei font o di FS2.

Non è un ripristino dell'intero kernel 10.2.4. La regressione del mouse sul PC
fisico segnalato resta da confermare dopo questo rollback: i risultati QEMU
non certificano quel controller o dispositivo.

## «Tabella non riconosciuta»

La verifica anticipata della firma MBR `55 AA` impediva anche di leggere una
GPT valida. Il riconoscimento GPT precede ora tale rifiuto, senza autorizzare
scritture indiscriminate. Solo il nuovo installer, su settori logici da 512 byte,
può proporre il ripristino della firma quando:

- entrambe le copie GPT e i rispettivi array passano i CRC e sono concordanti;
- esiste esattamente una voce MBR protettiva coerente, senza MBR ibrido o voci
  residue ambigue, partizioni sovrapposte o geometria non supportata;
- esiste una voce GPT libera e un intervallo realmente non allocato sufficiente.

La schermata richiede **«Confermo destinazione e ripristino firma MBR»**.
Preflight e annullamento non scrivono. La firma viene ripristinata soltanto
all'attivazione finale, dopo copia e verifica, insieme alle normali scritture
di avvio/nuova voce GPT; il settore MBR originale è conservato nel nuovo extent.
Non è una riscrittura della sola firma isolata da un'installazione.

Utility Disco e gli editor ordinari restano protetti. GPT discordanti,
corrotte, senza voce protettiva o ibride non vengono rese installabili. EFI,
MSR, NTFS e RAW rimangono occupati: nessuno è convertito in spazio libero.
Gli intervalli positivi inferiori a 1 MiB sono mostrati come `<1 MiB` anziché
`0 MiB`. Le righe destinazione rappresentano dischi, non singole partizioni.

La firma mancante è un caso riprodotto e corretto, **non una diagnosi dimostrata
del disco fisico dell'utente**. Se il motivo è diverso, l'installer deve ancora
rifiutare le scritture, mostrando una ragione specifica.

## Preservato e limiti

- Famiglia UI fornita e Mono Regular per Terminale/panic; nessun fallback
  DejaVu, griglia Terminale da 8 px conservata.
- FS2 di produzione, installazione/avvio 4Kn, Recovery e conversione/
  disattivazione della cifratura sullo stesso extent.
- Nessuna modifica alla visibilità dei file EFI/Recovery.
- Nessun Safari/WebKit completo incluso; nessuna certificazione Windows reale
  o compatibilità hardware universale. La ISO non è USB-DD ibrida.

Risultati della nuova build: `VALIDAZIONE-10.2.5-30B19.md` e
`workspace/local-10.2.5/hotfix/verification-30B19.json`. Le prove 30B18 restano
storiche, non valgono come prove fresche della 30B19.
