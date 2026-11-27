# 10.3 M2 — dimensioni, spazio, nomi

Checkpoint di sviluppo, Safari escluso. Nessun tag/release 10.3; nessuna modifica alle risorse dei font o al percorso USB canonico. I numeri di versione 10.2.6/30C1 sul supporto d'integrazione sono ancora intenzionalmente quelli della base.

## Causa e correzione dello spazio residuo

L'installer in coesistenza limitava il contenitore a 196608 settori (96 MiB), anche quando il partizionatore selezionava un gap molto più grande. Ora, **per nuove installazioni FS2**, il contenitore v2 occupa il gap autentico scelto dallo scanner, arrotondato per difetto all'allineamento di 2048 settori. Prefisso FAT e immagini di coda conservano dimensioni fisse; FS2 riceve l'estensione centrale reale. Non si prendono settori da partizioni esistenti, RAW o Windows. Non si espandono automaticamente volumi esistenti. Descrittori v1 ancora leggibili; backend legacy ancora a geometria v1.

Test host normali e ASan/UBSan: installazione effettiva GPT/MBR con limiti di scrittura, hash FAT32/NTFS/RAW invariati, backup/reset cifrati, rifiuto sorgenti invalide e attivazione impedita dopo errore. Suite installer legacy normale e sanitizzata PASS. Test geometria da minimo a 1 TiB, descrittori malformati e compatibilità v1 PASS.

## Dimensioni e interfaccia

- Formatter unico SI (1000, non 1024): Utility Disco, Installer, Finder/badge, Impostazioni, Time Machine, Informazioni e comandi interessati. Rimossa la doppia unità.
- Input distruttivo GUI/CLI in MB decimali: fino a sei decimali, punto o virgola; conversione verificata e allineata per difetto. Conferma mostra dimensione effettiva. Test di round-trip esatto, overflow e limiti PASS.
- DiskInfo espone geometrie del layout verificato; Utility Disco non inventa più `mb-45`. Barra del disco tratta le vere partizioni, grigio per spazio non allocato.
- Snapshot: niente dimensione fissa inventata o data zero presentata come reale. La dimensione condivisa CoW non viene spacciata per occupazione esclusiva.
- Rimosso pulsante/shortcut duplicato «Copia dati»: checkpoint FS2 e snapshot sono lo stesso meccanismo. Conservati lettura/ripristino delle copie esistenti; esportazione exFAT resta M5.
- Home visualizzata con nome account in Finder e selettore file. Percorsi persistenti rimangono invariati: nessuna rinomina cieca di file cifrati. Test selettore verifica etichetta Giulia e percorso HOME invariato.

## Prova guest reale sul supporto d'integrazione

ISO SHA-256: `09ea821f864998e3373e0741ab3f4685dc797f1b9a582953e9fe27b18225a5de`.

`pmbr_signature_guest.py`: installazione da GUI con veri dispositivi virtuali, su GPT sintetica con quattro partizioni preesistenti e firma PMBR assente. Nessuna formattazione host della destinazione nativa. Consenso esplicito e annullamento senza scritture; firmware non aveva riparato preventivamente il PMBR. Verificati descrittore v2, partizione che copre **20971520 settori** (intero gap 10 GiB), FS2 di **20848640 settori** nella regione dati, quattro entry GPT e confini occupati invariati, MBR originale conservato. Avvio NVMe a freddo senza ISO e accesso ad account/file importato PASS.

Build completa PASS, rilettura FAT 4096 indipendente 27+3 percorsi PASS. Foundation gate esteso con test reali del rendering Utility Disco normale/sanitizzato PASS. Overlay 28 sorgenti / 11 ABI sincronizzati. Evidenze in `m2-evidence`, risultato in `m2-guest-result.json`, log in `logs/m2-*`.

## Limiti e seguito

Questa non è una certificazione del Windows o del PC fisico dell'utente. Non è un'operazione di recupero del gap su installazioni già esistenti. Il backend mantiene il limite preesistente **1 TiB**, rifiutando capacità superiori: non si dichiara supporto >2 TB. Matrice hardware, capacità/4Kn completa e Recovery su tutte le varianti restano gate di rilascio; il test d'integrazione qui documentato è NVMe 512-byte logici. Nessuna promessa di completamento M3–M7. Prossimo macroblocco: M3, contabilità CPU/RAM.
