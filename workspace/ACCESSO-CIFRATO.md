# Ripristino accesso GitHub CLI

Le credenziali sono conservate **solo cifrate** in `private-backup/github-access.enc`.
La directory è esclusa da Git, ma resta tra i normali file del workspace.
Non è inserita nelle ISO o nel volume di sistema.

La chiave casuale è stata consegnata separatamente nel file `tmc-workspace-vault-key.txt`:
va scaricata e conservata dall’utente, fuori dal workspace e dal repository.
Senza chiave il backup non può essere decifrato; non è previsto un accesso automatico.
Non salvare qui la chiave o token in chiaro.

## Ripristino

1. Ripristinare GitHub CLI in una directory esterna, ad esempio `/var/tmp/tmc-github-cli`.
2. Eseguire `python3 workspace/credential-vault.py restore --config /var/tmp/tmc-github-auth`.
   La chiave viene richiesta senza eco nel terminale. In automazione è disponibile
   `--key-file /percorso/esterno/chiave.txt`: per sicurezza il programma rifiuta chiavi sotto `/home/user`.
3. Usare `GH_CONFIG_DIR=/var/tmp/tmc-github-auth` con la CLI e verificare l’account tramite `gh api user --jq .login`.
4. Usare `workspace/github_sync.py` per il confronto, poi `--push` solo quando autorizzato.

Formato autenticato: AES-256-CBC con PBKDF2-SHA256 (600.000 iterazioni), salt casuale OpenSSL,
più HMAC-SHA256 con chiave derivata separatamente e salt casuale indipendente, verificato prima della decifratura.
Testati round trip, password errata, archivio manomesso e accesso reale con credenziali ripristinate.
Le credenziali decifrate e i metadati Git restano fuori dal workspace.
Se il token è revocato o scaduto occorre una nuova autorizzazione, anche disponendo della chiave.
