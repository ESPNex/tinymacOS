# Dischi e TinyFS

## Layout installato

Il disco usa settori logici da 512 byte, GPT primaria e di backup, ESP FAT16, volume dati, partizione Recovery e sorgente sigillata per la reinstallazione. Il bootstrap BIOS usa il ponte MBR e lo spazio riservato prima dell'ESP; non cambia il disco in uno schema MBR tradizionale.

Le dimensioni e gli offset correnti sono definiti nel codice di storage e negli strumenti di generazione. Non usare le tabelle di Riviera/Levante dei documenti storici per modificare un disco Tramontana. L'installatore accetta soltanto le destinazioni vuote previste e rilegge i dati scritti prima di pubblicare la struttura.

## Namespace

Il catalogo di sistema espone risorse immutabili sotto `/System`, `/Applications`, `/bin`, `/sbin` e `/usr`. Bundle, font e immagini sono file reali del catalogo, non soltanto icone nel Finder.

TinyFS conserva i dati utente e le preferenze, con un profilo stabile sotto `/Users/User`. Nomi e percorsi hanno limiti statici; la capacità del disco non rende il filesystem equivalente a un filesystem general purpose. Il CD usa un volume dati in RAM per la sessione live. Finder e Terminale vedono lo stesso namespace.

## Manutenzione

La sorgente sigillata del sistema e la copia dei dati sono distinte. La prima consente la reinstallazione offline; la seconda permette il ripristino dei documenti. Recovery controlla integrità, identità e geometria prima delle operazioni previste.

La copia dati sullo stesso disco non è un backup contro guasti fisici. Non sono disponibili APFS/HFS+, journaling o FileVault. I controlli non garantiscono rollback dopo interruzione dell'alimentazione né autenticazione contro un attaccante con accesso completo al disco. Per le prove usare nuove immagini virtuali; conservarne una copia esterna a VM spenta.

Riferimenti di implementazione: `fs/`, `storage/`, `tools/system_volume.py` e `tools/bios-image.py`, relativi alla directory di sistema.
