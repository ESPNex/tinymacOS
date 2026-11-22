# Validazione

## Cosa dimostrano le prove

I contratti host verificano parser e invarianti; le prove guest esercitano il codice eseguito in QEMU; il collaudo fisico riguarda solo l'hardware realmente provato. Nessuno dei tre livelli sostituisce gli altri.

Ogni risultato va associato a sorgenti, kernel e media effettivi. Le immagini diagnostiche non sono le ISO distribuite. Non riutilizzare il risultato di una vecchia build per dichiarare collaudata una modifica nuova.

## Ultima build pubblicata

La 10.1.5 / 30B6 ha un gate di 80 ricevute, con 27 profili input live/installato, 236 asserzioni native, 18 avvii Recovery e 29 regressioni kernel. Cinque media QA e tredici profili guest sono associati al kernel di produzione. Sono risultati software, non conferma del funzionamento del mouse sulla ASRock.

Le evidenze sono in `workspace/release-10.1.5-30B6/` dalla radice del repository:

- `final-qa.json`: esiti e associazione agli hash.
- `QA-NOTES.md`: correzione della precedente associazione fra launcher e media.
- `github-release-verification.json`: verifica della pubblicazione e del download, distinta dai test.

Le ricevute restano riferite al commit sorgente e ai media registrati, anche quando `main` riceve successive correzioni documentali.

## Ripetere i controlli

Dalla directory di sistema, i contratti input indipendenti da QEMU:

```sh
bash tests/test-input-maintenance.sh
bash tests/test-legacy-input.sh
```

Le prove guest richiedono la toolchain, una build esterna e immagini disco usa-e-getta. Leggere prerequisiti e parametri dei singoli harness prima di avviarli. Il backend runtime ha una suite separata sotto `workspace/webkit-tmc/tools/`; i suoi PASS non certificano un browser WebKit.

Non rieseguire un vecchio `finalize-qa.py` o `publish.py` per certificare sorgenti diversi. Per un nuovo rilascio servono nuovi manifest e nuove prove; non modificare i JSON storici per adattarli al codice corrente.
