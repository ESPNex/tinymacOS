# tinymacOS 2.4.1 · build 21B2 — manutenzione

[Scarica la ISO da Google Drive](https://drive.google.com/file/d/1AkKo1USI3fTQbA9G2OE47yolmSwo6TuG/view)

**Non include Safari né WebKit funzionante.** È l'eccezione di manutenzione autorizzata prima del browser.

- File: `tinymacOS-2.4.1-x86_64.iso`
- Dimensione: 118.239.232 byte
- SHA-256: `e95428b77ca257f83636844d2dffdac6759a6305c9272ca67c2f8b3fcba660e6`
- MD5 Drive: `645a43df9a1ba0f73ce8b82653395f6c`
- Dimensione e SHA-256 letti dai metadati Drive dopo upload: coincidono con il file locale.
- Permessi di condivisione non modificati. Il collegamento richiede gli accessi correnti del tuo Drive.

## Correzioni e prove

1. **Mouse:** restano supportati i mouse USB boot-protocol; aggiunto decoder report-protocol per mouse relativi, report ID, assi firmati fino a 16 bit, primi tre pulsanti e rotella. Tablet assoluti conservano il percorso dedicato. Test parser host con AddressSanitizer/UndefinedBehaviorSanitizer: 160 combinazioni tablet, mouse relativo 16 bit/report ID/troncamenti PASS.
2. **QEMU, due avvii separati:** solo usb-tablet oppure solo usb-mouse. Cursore confrontato con lo sprite reale; clic sui tre campi account e Crea account; desktop; trascinamento Finder di +87,+41; menu clic destro. Tablet: doppio clic installer e rotella terminale. Recupero autonomo: selezione e avvio Terminale con clic, sw_vers 2.4.1. Tutto PASS; `evidence/pointer.json`.
3. **Password:** tolto il caret nei campi segreti; focus azzurro attenuato. Screenshot prima/dopo. Non ho riprodotto con certezza la grossa riga segnalata: la baseline mostra bordo bianco e puntini. Il cambiamento non dimostra la causa del difetto sul tuo PC.
4. **Recupero:** titolo alto 43 px, angoli raggio 18 e luci 13 px con geometria condivisa col desktop. Rasterizzazione antialias, rimosso il doppio disegno difettoso delle luci. Trascinamento e rilascio reali: contenuto traslato esattamente di +20,+20 pixel nel test. Rosso apre conferma di spegnimento; minimizza e zoom sono disabilitati/grigi nel runtime UEFI, non finti controlli attivi. Non è il compositor desktop né ne riproduce tutti gli effetti.
5. **Boot:** catturato il logo del loader tinymacOS, distinto dal logo firmware OVMF. Regione suggerimento: 856 pixel non neri prima, 0 dopo. R continua a entrare in Recupero nei test; F12 conservato nel codice.
6. **Media/crypto:** SHA-256 e PBKDF2 contro hashlib PASS. Cataloghi, digest KernelCache, W^X, DMG Recovery e sorgenti installer coerenti PASS. Nessun browser nei cataloghi.
7. **WebKit:** overlay sincronizzato e rieseguite le 7 verifiche backend/aspetto/compilazione target: PASS. ABI di rete nuove incluse nella manutenzione ma nessun test rete guest end-to-end o motore JavaScript/HTML.

## Rufus / hardware

Usa una chiavetta sacrificabile: Rufus cancella i dati della destinazione. Seleziona ISO, **GPT**, **UEFI senza CSM**, **FAT32**, modalità **ISO** quando richiesta; **Secure Boot disattivato**. Non prescrivere DD: questa ISO non è un'immagine disco hybrid. Questa procedura è basata sul formato auditato e sul precedente boot QEMU della chiavetta estratta; non ho eseguito Rufus su Windows né provato questa release sul tuo computer fisico.

Restano limiti driver: primo controller xHCI, dispositivi root-port, niente hub/hotplug generale. Ricevitori compositi, più interfacce e descrittori HID complessi non sono universalmente supportati. Per una prova preferire mouse cablato collegato direttamente, non attraverso hub. Se continua a non andare servono VID/PID e topologia/descrittore USB del dispositivo.

Il sistema sperimentale non aggiorna automaticamente un vecchio disco. Conserva un backup esterno dei dati. Non installare su dischi importanti.

## Sorgenti e rebuild

Sorgenti completi OS estratti: `/home/user/tinymacOS/`. Browser/backend estratti: `/home/user/workspace/webkit-tmc/`. Copie dei file modificati rispetto alla baseline: `sources/`; `source-sync.json` contiene i digest. Tutti i sorgenti compilati erano già sotto /home/user prima della build. Nessuna ISO/ZIP sotto /home/user; i media sono esterni e ricostruibili, non persistono nello snapshot. Il backup locale dei sorgenti non è prova di conferma snapshot della piattaforma.

```sh
cd /home/user/tinymacOS
python3 tools/local-tools.py /var/tmp/tmc/tools/root
chmod u+x build.sh run.sh tools/run-qemu.sh
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc-maintenance-241 bash build.sh
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc-maintenance-241 \
  python3 tests/riviera_pointer.py /var/tmp/nuovo-collaudo-241 --profile both
python3 /home/user/workspace/webkit-tmc/tools/run-tests.py
```

Gli script QA aggiuntivi in `qa/` usano i percorsi baseline/build espliciti riportati nei file; per rigenerare il confronto prima serve anche una build 2.4.0. La sincronizzazione sorgenti non garantisce riproducibilità binaria su toolchain arbitrarie.
