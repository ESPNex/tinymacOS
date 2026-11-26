"""Independent hashlib + libsodium verification of the production FV003 vector.
No imports of the C implementation, no exported plaintext volume key.
"""
import hashlib,struct,sys
from pathlib import Path
from nacl.bindings import crypto_aead_xchacha20poly1305_ietf_decrypt as decrypt
raw=Path(sys.argv[1]).read_bytes();assert len(raw)==512+4096+16384*2+64
h=raw[:512];m=raw[512:4608];c=raw[4608:20992];plain=raw[20992:37376];recovery=bytes.fromhex(raw[37376:].decode())
assert h[:8]==b'TMCFV003' and struct.unpack_from('<II',h,8)==(3,2)
assert hashlib.sha256(h[:480]).digest()==h[480:]
uuid=h[48:64];keys=[]
for slot in range(2):
 s=h[64+88*slot:64+88*(slot+1)];salt,nonce,wrapped,tag=s[:16],s[16:40],s[40:72],s[72:88]
 wrapping=hashlib.pbkdf2_hmac('sha256',b'scalable-block-password',salt,600000,32) if not slot else hashlib.blake2b(salt,key=recovery,digest_size=32).digest()
 aad=uuid+b'FV2KEY03'+struct.pack('<Q',slot)
 keys.append(decrypt(wrapped+tag,aad,nonce,wrapping))
assert keys[0]==keys[1];key=keys[0]
assert hashlib.blake2b(h[:240],key=key,digest_size=32).digest()==h[240:272]
assert m[:8]==b'FV2BLK03' and m[24:40]==uuid and not any(m[56:])
block,counter=struct.unpack_from('<QQ',m,8);assert block==0 and counter>0
assert decrypt(c+m[40:56],uuid+b'FV2DAT03'+struct.pack('<Q',block),uuid+struct.pack('<Q',counter),key)==plain
print('PASS independent libsodium + hashlib: actual 600000-round password slot AND recovery slot unwrap the same random volume key; header MAC/SHA and 16KiB XChaCha20-Poly1305 ciphertext/AAD/nonce verified')
