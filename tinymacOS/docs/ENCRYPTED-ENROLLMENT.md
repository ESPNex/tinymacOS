# Macroblocco: installazione cifrata, enrollment e reset FV003

Stato di sviluppo **10.2.5 / TMC_FS2_BACKEND=1**. Non è una pubblicazione e non
abilita FS2 per impostazione predefinita. La ricevuta del macroblocco, quando
completa, è `workspace/local-10.2.5/encrypted-enrollment-verification.json`.
Le ricevute precedenti descrivono esclusivamente i propri artefatti.

## Installazione nuova, non conversione

L'assistente live mantiene il percorso plaintext precedente. Alla conferma della
destinazione, **Proteggi con FileVault…** (anche tasto E) prepara un volume FV003:

1. Password del volume e conferma, da 8 a 64 caratteri, mascherate. La password
   dell'account importato rimane distinta; usare la stessa password non è imposto.
2. Entropia della produzione e chiave di recupero indipendente mostrate PRIMA di
   ogni scrittura al target. Occorre ricopiare gli ultimi otto caratteri. Anche il
   kernel richiede tale conferma e lega la preparazione al disco selezionato.
3. La conferma finale avvia il normale preflight dell'installer. Solo nello spazio
   nuovo non allocato si crea l'envelope, poi TMCBFS2, account, documenti e snapshot
   iniziale; rilettura e verifica precedono l'attivazione GPT/MBR.
4. La modalità richiesta dal client deve corrispondere a quella preparata nel
   kernel: un errore/annullamento non può trasformare silenziosamente una richiesta
   cifrata in una installazione plaintext. Tornare indietro o riprovare elimina
   password, chiave visualizzata e preparazione precedente.

L'API di creazione ricontrolla gli header persistiti vuoti. Non si usa mai come
fallback di mount, formattazione di volumi sconosciuti o conversione in-place.
Il costo dell'envelope è dichiarato: circa **80% del DATA grezzo utilizzabile**,
meno il prefisso e i metadati del filesystem. La capacità mostrata al filesystem
è quella realmente disponibile, non la capacità grezza del disco.

## Reset cifrato: politica esplicitamente distruttiva

Il vecchio reset generico continua a rifiutare FV003. Per il percorso nuovo:

- Avviare Recovery nativa e sbloccare il vecchio volume tramite FileVault.
- Aprire **Reinstalla**, selezionare il disco e scegliere **Inizializza dati
  cifrati…**. Questa opzione compare solo per un FV003 sbloccato e idoneo.
- Preparare una nuova password e confermare una nuova chiave di recupero prima
  della conferma distruttiva. Annullare non modifica DATA.
- Il kernel ricontrolla contesto Recovery, disco non montato, layout, integrità,
  stato sbloccato, credenziali preparate e conferma. Genera un volume con **nuova
  chiave dati e nuovo UUID**, non soltanto un account vuoto protetto dalle vecchie
  credenziali. Password e recovery slot vengono entrambi ricreati.
- I due header precedenti sono ritirati con marcatori riconoscibili prima della
  creazione dell'envelope nuovo. Quindi si ricreano home vuota, account vuoti e
  catalogo snapshot vuoto; si verifica il risultato prima del successo.
- Sistema, Recovery, tabelle delle partizioni ed extent estranei non vengono
  inizializzati. Al riavvio si sblocca con la nuova password del volume e si crea
  un nuovo account. Il nuovo account può avere una password distinta.

**Non è un reset atomico o annullabile dopo l'avvio.** Una interruzione I/O dopo
il ritiro degli header può rendere inaccessibile il vecchio volume e lasciare
quello nuovo incompleto. Si restituisce errore, mai successo o fallback plaintext.
L'interfaccia avverte prima di procedere. Non promette rollback o ripresa automatica.

**Non è cancellazione fisica sicura.** Si ritirano le chiavi sull'immagine logica
corrente; non si certifica l'eliminazione delle copie in flash/FTL, dei vecchi
header conservati esternamente o di immagini/backup posseduti da terzi. Rimane
valida la limitazione generale FV003: nessuna protezione contro replay di record
validi allo stesso indice o rollback dell'intero supporto. Non è un'attestazione
per cessione del supporto o sanitizzazione forense.

## Interfaccia e superficie consentita

`VaultInfo.format` distingue FV002, FS2 plaintext e FV003. Impostazioni non offre
la vecchia conversione/decifratura FV002 su FS2. Per FV003 rimangono sblocco,
cambio password e rigenerazione della recovery; per FS2 plaintext indica che la
cifratura si sceglie durante una nuova installazione. Il vecchio pulsante Recovery
per inizializzare indirizza al percorso con nuove credenziali, senza mutare DATA.
Nessuna attivazione/decrittazione/conversione in-place è dichiarata completata.

## Memoria e build pulita

L'installer e il resizer nativo legacy usano uno stesso workspace da NDATA settori,
anziché due buffer identici permanenti. I wrapper tengono `fs_gate`; `disk_edit`
e `disk_operation` rifiutano l'installer occupato. Non vi sono chiamate ricorsive
al resizer durante l'import del seed. Geometria verificata con `_Static_assert`.
Non si riducono capacità o limiti degli inode, non si allarga il limite ELF del
loader. I test effettivi del resize, dell'installer e dei dischi grandi devono
rimanere verdi. Il packaging legge l'icona installer dalla sorgente verificata,
non da un file temporaneo preesistente: funziona anche in una directory build nuova.

## Gate e limiti dell'accettazione

- Host reale GPT e MBR, normale + ASan/UBSan: nuovo installer cifrato, seed/account,
  snapshot, cold password/recovery unlock, validazione e annullamento senza
  scritture, rifiuto di doppia creazione e nessuna alterazione degli extent estranei.
- Reset host: nuove chiavi e UUID, rifiuto delle vecchie credenziali sul nuovo
  volume, account/documenti/catalogo vuoti, sblocco cold con nuove credenziali.
  Otto interruzioni nelle scritture reali verificano rifiuto/fail-closed e nessun
  falso successo; NON dimostrano atomicità o secure erase.
- Test UI dell'assistente: password diverse rifiutate, conferma recovery richiesta,
  pulizia su Indietro, errore e annullamento della scansione. Modalità cifrata
  richiesta senza preparazione rifiutata prima delle scritture.
- `tests/encrypted_install_reset_guest.py`: ISO corrente, vera GUI e NVMe emulato,
  nessun host-format o import di DATA. Installazione, credenziali separate, Recovery
  backup/ripristino, reinstallazione con SHA DATA invariato, reset, nuovo setup,
  persistenza dopo reboot e assenza dei marcatori plaintext verificati offline.
  Le chiavi si leggono dai pixel del display senza memoria guest, clipboard,
  log o screenshot di evidenza contenenti segreti.
- Regressione del percorso plaintext sullo STESSO artefatto: install/reinstall,
  UI paginata e backup, reset con nuovo account. Non sostituire questa evidenza
  con una ricevuta di un'ISO precedente.

Per l'esito esatto leggere la ricevuta finale: elenca SHA, gate completati e
limiti. Non sono compresi hardware fisico, conversione in-place, backup esterno,
boot/installazione nativi 4Kn o l'intera matrice di rilascio. Nessun push, merge,
tag o deploy 10.2.5; la release pubblicata 10.2.4 resta invariata.

## Esito finale del macroblocco — 2026-11-18

**PASS**: build completa in directory nuova; **8 gruppi host**, normale e
ASan/UBSan; e **4 gate guest sull’esatta stessa ISO**: installazione cifrata/reset,
installazione plaintext/reinstallazione, backup/UI paginata e reset plaintext.
I gate guest sono esclusivamente QEMU/NVMe emulato, non hardware fisico.

- ISO: `10902aa07f56b54f882cb3394d23ca22d6d9f22c8ff931ae7cf7495c7cac8178`
- KernelCache: `cdf87792c2da0c6ad57d0737d0f980a96c6ba7cff722ee867a4c72410d09d2fc`
- Kernel e Recovery: fine segmento `0x9dff5e4`, **2099740 byte** sotto il
  limite del loader, che non è stato aumentato.
- Plaintext: Finder/selettore raggiungono file 1099; Time Machine verifica lo
  snapshot 20; TextEdit legge/salva 128 KiB con SHA esatto; backup e reset con
  nuovo account rimangono funzionanti sullo stesso artefatto. Gli stress file
  sono aggiunti dall’host dopo installazione reale, non creati in massa dal guest.
- Cifrato: creazione/import completamente nell’installer guest, conferma chiave
  PRIMA delle scritture, login separato, recovery credential, backup/ripristino,
  reinstallazione con SHA DATA invariato, reset con nuove chiavi/account, vecchie
  credenziali rifiutate, persistenza e scansione offline dei marcatori verificati.
- Le prove d’interruzione del reset sono sul vero adapter host: NON dimostrano
  atomicità, sanitizzazione fisica o recuperabilità dopo perdita di alimentazione.

Ricevuta persistente: `workspace/local-10.2.5/encrypted-enrollment-verification.json`.
Log e anteprime senza chiavi nella directory adiacente `encrypted-enrollment-evidence`.
Matrice riproducibile: `bash tests/encrypted-enrollment-matrix.sh`, con i normali
prerequisiti di build/QA e una directory TMC_BUILD esterna. Nessuna release/deploy.
