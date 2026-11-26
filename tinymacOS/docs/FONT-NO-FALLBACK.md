# Font attivo senza fallback — modifica sorgenti successiva alla 30B17

Richiesta: eliminare il vecchio font errato e il fallback DejaVu.

## Modifiche effettive

- `assets/type.tfn` è interamente rigenerato dai quattro OTF upright forniti
  dall'utente, conservati in `assets/font-source/tinymacOS/`.
- Nessuna faccia bitmap viene copiata dal vecchio TFN2. Anche la faccia 5 del
  Terminale è ora Regular della famiglia fornita, adattato alla griglia di 8 px.
  Non è un font monospace progettato separatamente: le lettere larghe vengono
  compresse orizzontalmente quando necessario.
- Rigenerato anche `panicfont.inc` (ASCII 8×15 di emergenza del kernel) dal
  Regular fornito: nessuna eredità bitmap di provenienza non verificata.
- Nessun fallback di famiglia: Command, ricerca e backspace hanno disegni
  geometrici originali nel convertitore; soft hyphen usa il trattino della
  stessa famiglia. Un altro carattere mancante interrompe la conversione.
- Rimossi i generatori obsoleti `add-italian-glyphs.py`, `tramontana-font.py`
  e le sorgenti Display-Semibold inutilizzate. `art.py` non scrive più il font:
  rigenerare gli artwork non può ripristinare DejaVu.
- Lock aggiornato: il packager rifiuta il vecchio TFN2, compreso quello 30B17
  che conteneva ancora simboli DejaVu e la vecchia faccia monospace.
- Rimosse le due copie TFN sciolte errate note con hash 17b8004b… negli scratch
  locali. Le vecchie ISO e i commit storici non sono stati riscritti.

## Verifiche eseguite

- Conversione ripetuta con apertura font limitata ai quattro OTF del progetto:
  risultato identico byte per byte al nuovo asset; nessun font del sistema host.
- ABI: 11 facce, 207 glifi per faccia, altezze esistenti, tutti gli offset entro
  i limiti; simboli non vuoti; ogni glifo del Terminale ha avanzamento 8 px.
- Test reale C del lettore metriche utenti: PASS, incluse letture parziali,
  UTF-8, cache, header danneggiati e apertura rifiutata.
- Contratto hotfix e cleanup: PASS, con tre rimozioni di sorgenti obsolete
  autorizzate esplicitamente e hash della baseline originale mantenuti.

Questi sono test host di asset e metriche. **Non** sono una nuova validazione
visuale guest, dell'installer o della ISO. La ISO 30B17 già pubblicata contiene
ancora il precedente font; non è stata ricostruita né ripubblicata in questo
passaggio. I suoi 46 controlli restano riferiti a quell'artefatto storico.

Il pacchetto fornito non contiene una licenza: i metadati `tinymacOS` non
provano autore, derivazione da Inter o diritti di redistribuzione. Nessuna
licenza è stata inventata o attribuita alla nuova famiglia.

## Ricostruzione

```sh
python3 tinymacOS/tools/interface-font.py \
  tinymacOS/assets/font-source/tinymacOS tinymacOS/assets/type.tfn \
  --panic-output tinymacOS/assets/panicfont.inc
python3 tinymacOS/tests/test-font-no-fallback.py
```

Una modifica voluta del font richiede l'aggiornamento esplicito del lock e la
validazione dei media prima della prossima pubblicazione.
