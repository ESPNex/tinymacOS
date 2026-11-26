# Canali di pubblicazione

Le fasi sono indicate **nei tag**, non nei titoli, che restano `tinymacOS versione`.
È una classificazione editoriale prudente delle descrizioni esistenti, non un nuovo collaudo o una certificazione hardware. Tutte restano `prerelease=true` su GitHub, che non offre tre stati nativi distinti.

- **Developer Preview**: anteprime iniziali, regressioni fisiche bloccanti dichiarate o un sottosistema esplicitamente sperimentale.
- **Beta**: funzioni integrate e prove host/guest, con limiti e riscontri hardware ancora aperti.
- **Pre-Release**: indicazione esplicita nelle note (10.2.4) o gate software finali completati (10.2.5); non significa stabile o Safari incluso.

| Release | Fase | Tag corrente |
|---|---|---|
| tinymacOS 10.2.5 | Pre-Release | [`v10.2.5-pre-release`](https://github.com/ESPNex/tinymacOS/releases/tag/v10.2.5-pre-release) |
| tinymacOS 10.2.4 | Pre-Release | [`v10.2.4-pre-release`](https://github.com/ESPNex/tinymacOS/releases/tag/v10.2.4-pre-release) |
| tinymacOS 10.2.3 | Beta | [`v10.2.3-beta`](https://github.com/ESPNex/tinymacOS/releases/tag/v10.2.3-beta) |
| tinymacOS 10.2.2 | Beta | [`v10.2.2-beta`](https://github.com/ESPNex/tinymacOS/releases/tag/v10.2.2-beta) |
| tinymacOS 10.2.1 | Beta | [`v10.2.1-beta`](https://github.com/ESPNex/tinymacOS/releases/tag/v10.2.1-beta) |
| tinymacOS 10.2 | Beta | [`v10.2-beta`](https://github.com/ESPNex/tinymacOS/releases/tag/v10.2-beta) |
| tinymacOS 10.1.9 | Beta | [`v10.1.9-beta`](https://github.com/ESPNex/tinymacOS/releases/tag/v10.1.9-beta) |
| tinymacOS 10.1.8 | Beta | [`v10.1.8-beta`](https://github.com/ESPNex/tinymacOS/releases/tag/v10.1.8-beta) |
| tinymacOS 10.1.7 | Developer Preview | [`v10.1.7-usbhid-candidate-developer-preview`](https://github.com/ESPNex/tinymacOS/releases/tag/v10.1.7-usbhid-candidate-developer-preview) |
| tinymacOS 10.1.6 | Beta | [`v10.1.6-beta`](https://github.com/ESPNex/tinymacOS/releases/tag/v10.1.6-beta) |
| tinymacOS 10.1.5 | Beta | [`v10.1.5-beta`](https://github.com/ESPNex/tinymacOS/releases/tag/v10.1.5-beta) |
| tinymacOS 10.1.4 | Developer Preview | [`v10.1.4-developer-preview`](https://github.com/ESPNex/tinymacOS/releases/tag/v10.1.4-developer-preview) |
| tinymacOS 10.1.3 | Developer Preview | [`v10.1.3-developer-preview`](https://github.com/ESPNex/tinymacOS/releases/tag/v10.1.3-developer-preview) |
| tinymacOS 10.1.2 | Developer Preview | [`v10.1.2-developer-preview`](https://github.com/ESPNex/tinymacOS/releases/tag/v10.1.2-developer-preview) |
| tinymacOS 10.1.1 | Developer Preview | [`v10.1.1-developer-preview`](https://github.com/ESPNex/tinymacOS/releases/tag/v10.1.1-developer-preview) |
| tinymacOS 10.1 | Developer Preview | [`v10.1-developer-preview`](https://github.com/ESPNex/tinymacOS/releases/tag/v10.1-developer-preview) |
| tinymacOS 10 | Developer Preview | [`v10-developer-preview`](https://github.com/ESPNex/tinymacOS/releases/tag/v10-developer-preview) |
| tinymacOS 2.4.5 | Developer Preview | [`v2.4.5-developer-preview`](https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.5-developer-preview) |
| tinymacOS 2.4.4 | Developer Preview | [`v2.4.4-oc2-developer-preview`](https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.4-oc2-developer-preview) |
| tinymacOS 2.4.4 | Developer Preview | [`v2.4.4-oc1-developer-preview`](https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.4-oc1-developer-preview) |
| tinymacOS 2.4.4 | Developer Preview | [`v2.4.4-developer-preview`](https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.4-developer-preview) |

I tag Git precedenti sono conservati **immutati come alias dei sorgenti** per non spezzare i riferimenti ai commit e le ricevute storiche. La release e i download usano il nuovo tag con suffisso. Non sono stati spostati commit, ricreati asset o modificati titoli/descrizioni/checksum. La 10.2.5 mantiene una sola ISO BIOS+UEFI.

La **10.3 è in sviluppo**: nessuna release o ISO 10.3 è pubblicata. Il lavoro Safari continua in `main`; motore WebKit e UI completa restano gate obbligatori, non sostituibili con test del solo runtime.
