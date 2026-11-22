# QA 10.1.6 / 30B7

## Ambito

Release di pulizia dell'interfaccia e correzione delle risorse iconografiche. Non modifica i driver USB/PS2 né il percorso firmware della 10.1.5. Nel codice incorporato nel kernel cambia la versione del testo Welcome.txt; non la logica di input, scheduler o memoria. L'utente ha confermato avvio e USB funzionanti in VirtualBox con la 10.1.5; il successo non è una prova sulla ASRock Q1900M/J1900 fisica.

## Riproduzione

I due output esterni sono `/var/tmp/tmc-10.1.6/build` e `/var/tmp/tmc-10.1.6/repro`; ogni ISO nativa è confezionata separatamente da `tools/dualboot-iso.py`. `artifact.json`, `source-hashes.json` e `SHA256SUMS.txt` identificano il risultato. I percorsi esterni non sono distribuiti né necessari per leggere le ricevute.

Suite della release, dalla radice:

```sh
python3 workspace/release-10.1.6-30B7/run-host-qa.py
bash workspace/release-10.1.6-30B7/run-input-qa.sh
bash workspace/release-10.1.6-30B7/run-kernel-optical-qa.sh
```

Dopo l'identità ISO: `ui-icons-qa.py`. Controlli aggiuntivi: `svg-contract.py`, `firmware_contract.py`, `async-retry-contract.py`, `save-preview.py`. `finalize-qa.py` fallisce se manca una ricevuta, un hash non coincide o una prova è fallita. La conversione delle release precedenti in prerelease è una verifica editoriale separata, non un test del sistema operativo.

## Nuova accettazione interfaccia

- App Store datato 2026-09-23: hash del PNG originale, slot 13 dell'atlante, risorsa nei cataloghi live/installabile e pixel opachi delle icone Finder a 19 e 54 px.
- Utility Disco: byte RGBA64 identici fra slot OS 39 e sottoinsieme Recovery, presenza nell'EFI estratto dalla ISO e confronto dei pixel effettivamente disegnati in Recovery.
- Conservazione dei simboli procedurali del filesystem e degli SVG originali del Centro di Controllo.
- Assenza dei vecchi comandi di aspetto duplicati nel codice e nel desktop collegato; menu contestuale reale, apertura di Utility Disco e regressione completa dei controlli per tema/sfondo.

Le schermate conservate sono framebuffer reali; non sono ricostruzioni. Le ricevute JSON riportano i risultati effettivi: questo documento descrive le prove, non le sostituisce.

## Kernel e input

I media diagnostici sono distinti dalla ISO retail. Ogni ricevuta dei cinque probe collega hash di kernel e KernelCache alla produzione; i tredici profili SMP vengono verificati contro il rispettivo media QA. I test input selezionano esplicitamente il trasporto attivo per non confondere il fallback PS/2 con USB.

## Tentativi e correzioni degli harness

Il primo tentativo live UEFI `xhci-hub-kbd` ha fallito il confronto immediato della posizione dopo un trascinamento, dopo avere superato topologia, login e movimento indipendente X/Y. Ricevuta e log sono conservati in `attempts/`. La causa non è stabilita: non è stato modificato il driver per far passare il test. La matrice è ripresa solo con ricevute nuove, positive e associate alla stessa ISO; `hub-drag-qa.py` aggiunge dodici trascinamenti e aspetta esplicitamente i pixel di destinazione, anziché accettare una qualsiasi vecchia etichetta ancora visibile.

Anche il primo tentativo separato Recovery UEFI/PS2 è rimasto al menu iniziale invece di aprire il Terminale. Ricevuta, log e framebuffer sono conservati in `attempts/`; la causa non è stabilita. La stessa prova, senza modifiche a sistema o harness, è passata nella matrice finale, insieme agli altri tre profili Recovery. Il riepilogo dei PASS non implica assenza di questi fallimenti intermittenti.

Il nuovo controllo icone è stato corretto durante lo sviluppo: la prima verifica di apertura poteva riconoscere la voce «Utility Disco» nel menu prima del cambio pagina; una seconda prova ha mostrato che serve un disco realmente installato, non un disco vuoto. La versione finale clona il volume usa-e-getta della prova installer e richiede il testo esclusivo della pagina Utility Disco. I confronti icone restano separati dall'apertura dell'utility.

Nessun risultato QEMU è una certificazione di hardware reale o di compatibilità HID universale. Safari/WebKit non è incluso.
