# M1 — chiusura dei gate tecnici del macroblocco

**Esito: M1 completo rispetto ai criteri locali host/guest di `tinymacOS/docs/PIANO-10.3.md`. M2 resta NON completo. Non è una release 10.3 e non è una certificazione hardware.**

ISO di collaudo (non pubblicata come release): `76d7a6b52b3167fd132434136503569241d1ae37cc63c4208a7369a1b0db8018`. Build e collaudi finali riguardano gli stessi sorgenti funzionali: hash in `source-manifest.json`, artefatti in `artifacts.json`. La ISO è conservata fuori dal workspace, in `/var/tmp/tmc-10.3/m1-native-build/tinymacOS-10.3-M1-native.iso`; non dipendere dalla persistenza di `/var/tmp` per le ricevute.

## Correzioni aggiuntive

- Terminale: `apfs_unlockfv` e `diskutil apfs unlockVolume`, senza argomento password e senza pipeline/redirezione, aprono un prompt dedicato senza eco. Nessuna lettera o pallino; stessa posizione del caret indipendentemente dalla lunghezza. Input UTF-8 limitato a 64 byte, cancellazione con Backspace, annullamento con Escape/Ctrl-C. Cronologia, selezione, clipboard e trascinamento non ricevono il segreto. Wipe volatile alla conferma, annullamento e chiusura.
- Il prompt usa FileVault whole-volume quando riconosciuto, mantenendo il vecchio percorso soltanto per il backend storico. Non passa la credenziale in argv a un processo Command. Autorizzazione syscall del Terminale limitata a **stato e sblocco**: non abilita attivazione, disattivazione, cambio password o blocco. La copia temporanea della richiesta e quella legacy nel kernel sono cancellate.
- Dialoghi condivisi: corretta la geometria di `inputmodal`, che collocava testo/caret con dimensioni diverse dal campo effettivamente disegnato.
- Nessuna modifica a USB, font, formato disco o numeri pubblici di versione. Nessun lavoro su M3.

## Matrice di accettazione

| Criterio | Prova eseguita | Esito |
|---|---|---|
| Wrapping UTF-8, larghezze, nessuna modifica implicita ai byte | Vera paint/TextEdit, suite foundation e nuovi round-trip a 758/460/1278/600 px | PASS normale + ASan/UBSan |
| File lunghi con/senza newline | 144.000 e 141.735 byte; I/O su veri file temporanei host | PASS normale + ASan/UBSan |
| Click, scroll indipendente, resize, Home/End, inserimento/Backspace/Delete | Funzioni reali dell'editor; fixture restaurata dopo le modifiche | PASS normale + ASan/UBSan |
| Round-trip sul filesystem nativo guest | Gli stessi due contenuti UTF-8 aperti, modificati, salvati, chiusi, riaperti e risalvati; `shasum -a 256` del guest prima/dopo; ridimensionamento reale verificato dalle coordinate del toolbar | PASS USB/QEMU |
| Caret vuoto, reset, pausa/lampeggio | Osservazione del solo rettangolo lampeggiante, freccia di reset e campionamento ad ogni tasto, massimo 300 ms per attendere il frame TCG | PASS USB/pixel |
| Digitazione continua, frecce, cancellazione | Terminale normale, TextEdit, Note e Memo; inclusi accenti UTF-8 | PASS USB/pixel |
| Focus/campi non attivi/overlay | Test compositor e Note/Memo; menu reale sopprime il caret nell'effettiva posizione finale; Tab nei campi setup/Installer/Recovery, click nel documento | PASS host sanitizzato + USB/pixel |
| Password grafiche | Campi vuoti e mascherati del setup, Installer e FileVault in Recovery; conferma via Tab; serializer host non contiene la password in chiaro | PASS host + USB/pixel |
| Terminale senza eco, nessuna lunghezza rivelata | Invarianza dell'intero stream Draw per digitazione/cancellazione UTF-8; isolamento da cronologia/clipboard/drop; overflow e wipe su successo/errore/Ctrl-C/Escape | PASS normale + ASan/UBSan |
| No-echo con backend reale, non soltanto mock | Seconda installazione **cifrata dalla GUI**: Recovery nativa senza ISO, password errata rifiutata, password corretta sblocca FV003; Impostazioni FileVault conferma indipendentemente `Attivo` | PASS USB/QEMU |
| Font, USB, overlay e compilazione integrata | Foundation, M0 `--workspace`, ricostruzione font senza fallback, build completa BIOS/UEFI/Recovery | PASS |

### Provenienza delle fixture e limiti

- I due documenti lunghi sono **input di test aggiunti offline dall'host soltanto dopo una vera installazione GUI**; non si dichiara di averli creati in massa dal Terminale. Il Terminale ha redirezioni limitate a 8 KiB, quindi non viene usato per fingere la creazione di file da 140 KiB. Apertura, modifiche, salvataggio, riapertura e hash successivi avvengono invece nel guest attraverso il filesystem nativo.
- La prova di sblocco FV003 non usa questa preparazione: installer, cifratura e import dell'account avvengono nella GUI guest. L'host legge soltanto la firma risultante; non formatta né cifra DATA.
- L'accettazione finale `m1-acceptance-result.json` è una **esecuzione fresca completa**, non un collage di tentativi ripresi. `m1-terminal-vault-result.json` è la prova cifrata aggiuntiva sulla medesima ISO.
- Tentativi preliminari hanno corretto il driver QA: campionamento prima del completamento del frame, ricerca del footer coperto dal puntatore, opzione SHA-256 omessa e limite della redirezione shell. Non sono stati conteggiati come PASS; le prove finali complete sono quelle qui conservate. Un primo comando di verifica font usava il percorso errato `tools/`; la verifica effettiva è `tests/test-font-no-fallback.py`, passata nel log finale.
- I test non dimostrano hardware fisico, tutte le risoluzioni GOP, prestazioni globali, M2 o gli altri macroblocchi. La matrice finale di release M7 resta distinta.

## Riproduzione

```sh
bash tinymacOS/tests/test-10.3-foundation.sh
python3 tinymacOS/tests/test-m0.py --workspace
python3 tinymacOS/tests/test-font-no-fallback.py
PATH=/usr/sbin:$PATH TMC_BUILD=/var/tmp/tmc-m1-build \
  TMC_ISO=m1.iso bash tinymacOS/build.sh
TMC_STAGE_ISO=/var/tmp/tmc-m1-build/m1.iso TMC_QA_DIR=/var/tmp/tmc-m1-qa \
  python3 tinymacOS/tests/m1_acceptance_guest.py
TMC_STAGE_ISO=/var/tmp/tmc-m1-build/m1.iso TMC_QA_DIR=/var/tmp/tmc-m1-qa \
  python3 tinymacOS/tests/m1_terminal_vault_guest.py
```

Occorrono le dipendenze QEMU/OVMF/Python già usate dalle altre suite guest. Usare solo target QA nuovi e sacrificabili, mai dischi dell'utente.
