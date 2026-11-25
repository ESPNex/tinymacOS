# Monocypher 4.0.2

Upstream: https://github.com/LoupVaillant/Monocypher/tree/4.0.2

Unmodified `src/monocypher.c`, `src/monocypher.h` and `LICENCE.md` from tag 4.0.2. Dual BSD-2-Clause / CC0 licensing; see LICENCE.md. Only XChaCha20-Poly1305, constant-time comparisons, BLAKE2b and secure wipe are used by the native volume format. Password KDF: existing PBKDF2-HMAC-SHA256 at 600,000 iterations; recovery credential: random 256 bits. No Apple FileVault/APFS interoperability is claimed.
