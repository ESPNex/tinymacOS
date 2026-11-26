# Pulizia completa dei tag — PASS

Su approvazione esplicita dell’utente, rimossi i **21 vecchi alias senza maturità** che puntavano agli stessi commit dei tag classificati.

- Prima: **46 tag**.
- Dopo: **25 tag su 25 commit distinti**, senza duplicazioni di commit.
- Conservati **21 tag con maturità**: 11 Developer Preview, 8 Beta, 2 Pre-Release. Due Developer Preview sono riferimenti storici delle varianti 2.4.4 archiviate, non ulteriori schede release.
- Conservati i **4 riferimenti storici unici**: `archive/v10.2.3-before-cow-82ec7bc`, `v2.4.5-21B6`, `v2.4.5-21B7`, `v2.4.5-21B8`.
- Tutte le **19 release** conservano esattamente nomi, descrizioni, assegnazione dei tag e asset. Una sola ISO 10.2.5, invariata. Nessun test software rieseguito per questa operazione sui soli riferimenti Git.

## Backup e verifica

Prima della prima rimozione è stata pubblicata la mappa completa di ripristino nel commit `1850f177d11210f36725f69a2305feb5e394aef4`, poi scaricata indipendentemente da GitHub e confrontata byte per byte. Include gli oggetti originali dei tag, anche quello annotato della 10.2.5.

- Mappa: `tag-alias-restore-map.json`.
- Ricevuta del controllo finale: `tag-deduplication-verification.json`.
- [Tag su GitHub](https://github.com/ESPNex/tinymacOS/tags).

**Nuova politica:** i 21 alias rimossi devono restare assenti; non ricrearli automaticamente durante futuri audit. Questa decisione sostituisce le precedenti note sulla conservazione/ripristino di tutti i vecchi alias, presenti nei resoconti storici. Il rollback va eseguito solo su richiesta esplicita. I link basati sui vecchi nomi rimossi possono non funzionare più; usare i tag classificati.
