"""Independent libsodium/Python verification of bytes produced by vault_host.c."""
from pathlib import Path
import hashlib,struct,sys
from nacl.bindings import crypto_aead_xchacha20poly1305_ietf_decrypt
raw=Path(sys.argv[1] if len(sys.argv)>1 else '/var/tmp/tmc-10.2.3/vault-vector.bin').read_bytes();h,cipher,t=raw[:512],raw[512:1024],raw[1024:]
assert len(raw)==1056 and h[:8]==b'TMCFV002'
assert hashlib.sha256(h[:480]).digest()==h[480:]
uuid=h[40:56];slot=h[56:144];salt,nonce,wrapped,tag=slot[:16],slot[16:40],slot[40:72],slot[72:88]
k=hashlib.pbkdf2_hmac('sha256',b'Native-pass-285',salt,600000)
key=crypto_aead_xchacha20poly1305_ietf_decrypt(wrapped+tag,uuid+struct.pack('<II',2,0),nonce,k)
assert hashlib.blake2b(h[:232],key=key,digest_size=32).digest()==h[232:264]
counter=struct.unpack_from('<Q',t)[0]
p=crypto_aead_xchacha20poly1305_ietf_decrypt(cipher+t[8:24],uuid+struct.pack('<Q',3),uuid+struct.pack('<Q',counter),key)
assert p==bytes((3*7+j)%251 for j in range(512))
print('PASS independent Python PBKDF2-HMAC-SHA256 and BLAKE2b authenticate the actual header; libsodium independently unwraps the master key and decrypts the actual XChaCha20-Poly1305 sector')
