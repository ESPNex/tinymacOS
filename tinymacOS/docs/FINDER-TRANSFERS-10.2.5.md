# Finder e Terminale — trasferimenti 10.2.5

## Finder

- Menu contestuale **Taglia, Copia, Incolla** per l'elemento selezionato:
  file oppure cartella con il relativo contenuto.
- Stesse azioni nel menu Modifica e con **Ctrl+X / Ctrl+C / Ctrl+V**.
- Appunti condivisi fra finestre/processi Finder. Taglia non elimina nulla:
  il trasferimento avviene con lo spostamento del filesystem soltanto a Incolla
  riuscito; dopo un errore gli appunti rimangono disponibili per riprovare.
- Incolla da tastiera usa la cartella corrente; Incolla dal menu contestuale
  di una cartella selezionata usa quella cartella.
- Copia in streaming con buffer da 8 KiB, senza il vecchio limite della
  dimensione del singolo file. Cartelle enumerate in pagine da 16 elementi,
  senza una tabella di tutti i figli. Profondità massima 32, più i normali limiti
  di percorso/nome del filesystem.
- Nessuna sovrascrittura automatica: collisioni producono `Copia nome`,
  `Copia 2 nome`, ecc. Un eventuale Taglia con collisione usa ugualmente un
  nuovo nome, senza sostituire l'elemento preesistente.
- Rifiuto di copie dentro la stessa cartella sorgente/discendenti, anche
  attraverso l'alias `/Volumes/Macintosh HD`; rispetto della sola lettura.
- In caso di errore durante una copia si tenta la rimozione del nuovo contenuto
  parziale. La sorgente non viene rimossa. Una copia non è una transazione
  atomica dell'intero albero rispetto alla perdita di alimentazione: può
  lasciare un destinatario incompleto dopo crash. Non è uno snapshot e non
  promette consistenza se un'altra applicazione modifica la sorgente durante
  la copia. Lo spostamento sul volume FS2 usa invece la pubblicazione atomica
  già prevista dal filesystem.

## Drag and drop

Un trascinamento parte dopo una soglia di movimento, non al semplice clic.
Supporta file/cartelle verso una directory visibile, sfondo di una finestra
Finder, barra laterale, un'altra finestra Finder e Scrivania. Anche le icone dei
file sulla Scrivania possono essere trascinate verso Finder. Le cartelle della
Scrivania sono destinazioni esplicite.

- Trascinamento ordinario: sposta un elemento scrivibile sul volume gestito.
- **Ctrl oppure Option/Alt durante il rilascio:** copia.
- Sorgente in sola lettura: copia, mai cancellazione della sorgente.
- **Esc** annulla prima del rilascio.
- Il trascinamento non sovrascrive gli appunti Copia/Taglia.
- Percorso e token di provenienza vengono correlati al processo Finder che ha
  iniziato il gesto. Risposte tardive non devono riutilizzare un altro gesto;
  la destinazione è quella delle coordinate registrate al rilascio.

Non è un'implementazione di selezione multipla, trascinamento nel Cestino,
importazione verso volumi esterni non montati o negoziazione di formati dati
arbitrari fra tutte le applicazioni.

## Terminale e appunti di testo

- **Ctrl+Shift+C / Ctrl+Shift+V / Ctrl+Shift+X**, menu Modifica e contestuale.
- Copia dell'intera riga di input, oppure di un intervallo di output selezionato
  con il mouse. Taglia svuota la riga di input; non cancella l'output di un
  comando, che rimane in sola lettura.
- **Ctrl+C resta l'interruzione del comando**, non viene riutilizzato per Copia.
- Incolla modifica soltanto la riga: ritorni a capo, tab e controlli diventano
  spazi; non vengono sintetizzati Invio né eseguiti comandi. Se il testo non
  entra nella riga, l'operazione viene rifiutata interamente.
- Trascinare un percorso da Finder nel Terminale lo inserisce fra virgolette,
  con escape di virgolette, backslash e caratteri d'espansione; non lo esegue.
- Appunti di testo condivisi con TextEdit e con `pbcopy` / `pbpaste`, fino a
  256 KiB. `pbcopy` accetta testo da una pipe; non accetta byte NUL.
- Gli appunti distinguono testo da elemento del filesystem; Finder non tratta
  un testo incollato come un file. Il blocco della sessione li svuota.

```
cp -R origine-cartella destinazione
cp origine-file destinazione
mv origine destinazione
echo testo | pbcopy
pbpaste
```

`cp` è ricorsivo con `-R`/`-r`, usa lo stesso motore streaming e rifiuta un
nome di destinazione già esistente. `mv` usa lo spostamento del filesystem e
rifiuta sostituzioni/cicli. Nessuna opzione di sovrascrittura forzata è stata
aggiunta.

## Prove riproducibili

- `bash tests/test-finder-transfers.sh`: motore reale su backend FS2,
  1.100 file, cartelle annidate, file da 256 KiB, collisioni, cicli/alias,
  sola lettura, tutti i 32 errori sui chunk di scrittura, rimontaggio;
  Terminale/appunti e regressione Finder/Time Machine/file chooser;
  build normale e ASan/UBSan.
- `bash tests/test-finder-menus.sh`: costruzione reale dei menu Finder/Terminale,
  inclusi più di dieci elementi contestuali, normale e ASan/UBSan.
- `tests/finder_transfers_guest.py`: gesto reale di tastiera/puntatore/menu
  sul supporto installato dalla GUI. La ricevuta con `passed=true` e gli hash
  dell'artefatto è necessaria per dichiarare passato il gate guest.

## Esito della verifica della funzione

**PASS**: entrambe le suite host in modalità normale e ASan/UBSan; regressioni
input-maintenance, legacy-input, fs2-runtime, fs2-install, installer-flow e
browser-pages. Sullo stesso artefatto sono passati l'installer GUI, l'avvio
installato e la reinstallazione Recovery con DATA invariato, poi tutti gli
**11 controlli GUI Finder/Terminale**:

1. `cp -R`, `mv`, `pbcopy`/`pbpaste` reali.
2. Copia/incolla contestuale di un file fra finestre, sorgente conservata.
3. Taglia/incolla contestuale di una cartella annidata fra finestre.
4. Trascinamento su una cartella e sulla Scrivania.
5. Trascinamento verso la Scrivania nella barra laterale.
6. Ctrl-drag fra finestre: copia con nuovo nome in caso di collisione.
7. Icona di un file sulla Scrivania trascinata dentro Finder.
8. Percorso trascinato nel Terminale: inserimento fra virgolette e lettura del
   file dopo Invio esplicito.
9. Copia/incolla da tastiera nel Terminale e condivisione con `pbpaste`.
10. Blocco della sessione, nuovo accesso e appunti svuotati.
11. Avvio installato a freddo, rilettura dei file trasferiti e verifica completa
    dei dati/del sistema con `diskutil verifyVolume disk0`.

ISO verificata, SHA-256:
`c2eb0a5f1ea3ea931d12048c380ca5d2f389aa691f0ac34b3b5c34e75ac62b3a`.
KernelCache:
`3d98e1118960bfd3c7b4267e5a8b7148ab6b87c72552e5119fa5afc58cad3f72`.

Ricevuta persistente: `workspace/local-10.2.5/finder-transfers-verification.json`;
log compressi, manifest sorgente del build e ricevute GUI in
`workspace/local-10.2.5/finder-transfers-evidence/`, sotto `/home/user`.
Gli ultimi aggiustamenti dopo il build riguardano soltanto il test GUI:
geometria sopra il Dock, confronto che evita il cursore, font della schermata
di blocco e isolamento dei nomi di prova. Nessuna modifica all'implementazione.
Le esecuzioni precedenti interrotte dal riconoscimento dello schermo non sono
conteggiate come riuscite; la prova completa finale è `DropQA3`.

**La funzione richiesta è implementata e verificata; la 10.2.5 non è pubblicata.**
Il macroblocco non autorizza automaticamente una release: rimane distinta
l'accettazione finale dell'intera 10.2.5. Queste prove sono in QEMU, non su
hardware fisico, e non sostituiscono la matrice finale della release.
