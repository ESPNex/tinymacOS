# tinymacOS 10.2.5 — build 30B18

Rebuild della 10.2.5 con la famiglia UI e il Mono forniti dall'utente.
Non è una release 10.3 e non include Safari completo.

## Font di produzione

- UI: Regular 400, Medium 500, Semibold 600 e Bold 700 del pacchetto
  `tinymacosfonts.zip`. Nessun peso sintetico o faccia bitmap ereditata.
- Terminale: vero `tinymacOSMonoRegular.otf` da `tinymacosmonofonts.zip`,
  13 px, altezza 21 px, celle/avanzamento di 8 px.
- Panic: lo stesso Mono Regular a 12 px genera il bitmap ASCII 8×15;
  invariato il raddoppio dei pixel eseguito dal kernel.
- Nessun font DejaVu o altra famiglia di fallback nel TFN2. Command, ricerca
  e backspace sono disegni geometrici originali. Soft hyphen usa il trattino
  della stessa famiglia; per Mono NBSP usa spazio e gli apici numerici 1/2/3
  sono cifre ridotte della stessa famiglia. Altre mancanze bloccano la conversione.
- I dieci volti UI diversi dal Terminale restano identici alla revisione
  senza fallback precedente al Mono: metriche e bitmap confrontati per glifo.
- Rimossi generatori/font obsoleti; la rigenerazione degli artwork non può
  sovrascrivere il font di produzione. Lock con hash rifiuta vecchi asset.

Provenienza e limiti di licenza: `licenses/FONTS.txt`. Gli ZIP dell'utente non
contengono una licenza esplicita: il nome interno non dimostra autore,
derivazione da Inter o diritti di redistribuzione. Non è stata inventata una
licenza né attribuita questa famiglia a Inter.

## Funzionalità conservate

Restano le correzioni mouse/code desktop e inventario dei veri gap su GPT
protette della 30B17. MSR/NTFS/RAW non diventano spazio libero; una tabella
non sicura resta protetta dalle scritture. Conservati drag-and-drop e
Taglia/Copia/Incolla, installazione e avvio 4Kn, cifratura e Recovery.

## Accettazione e distribuzione

Ricostruzione completa da sorgenti, non sostituzione del solo file ISO.
L'audit legge i cataloghi live/installato/Recovery e verifica il TFN2 corrente,
versione 30B18 e KernelCache live/installato comune. Verifica inoltre la
presenza del nuovo bitmap panic nei kernel effettivamente estratti da tutti
e tre gli ambienti. I risultati guest della nuova ISO sono distinti dai
precedenti della 30B17; i vecchi conteggi non vengono riattribuiti.

La pubblicazione viene eseguita solo dopo la chiusura dei controlli richiesti.
Esiti e hash definitivi: `workspace/local-10.2.5/hotfix/verification-30B18.json`
e asset pubblici `VALIDAZIONE-10.2.5.md` / `VERIFICA-FINALE-10.2.5.tar.gz`.

Resta una sola ISO BIOS+UEFI nella release `v10.2.5-pre-release`, con titolo
normalizzato. Non è un'immagine USB-DD ibrida. Nessuna certificazione di mouse
fisico o dual boot Windows reale; le prove guest sono QEMU e layout sintetici.
Le copie già distribuite e la cronologia Git non vengono cancellate.
