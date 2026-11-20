#!/usr/bin/env python3
"""Compare freestanding crypto against the standard library (no VM credentials involved)."""
import ctypes as c,hashlib,tempfile,subprocess,os
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as td:
 so=td+'/digest.so';subprocess.run(['gcc','-fPIC','-shared','-O2','-Iinclude','crypto/sha256.c','crypto/pbkdf.c','-o',so],cwd=root,check=True);lib=c.CDLL(so)
 for n in (0,3,55,56,63,64,65,8192,100003):
  b=os.urandom(n);out=c.create_string_buffer(32);lib.sha_bytes(b,n,out);assert out.raw==hashlib.sha256(b).digest()
 for n in (1,2,4096,120000):
  p=b'public unit test vector';salt=bytes(range(16));st=c.create_string_buffer(512);lib.pbk_begin(st,p,len(p),salt);lib.pbk_run(st,n-1);# SHA256 struct is 104 bytes; two states, U and T
  assert st.raw[240:272]==hashlib.pbkdf2_hmac('sha256',p,salt,n)
print('SHA-256 (9 lengths), PBKDF2-HMAC-SHA256 (4 iteration counts): match hashlib.')
