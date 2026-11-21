#!/usr/bin/env python3
"""Authenticated encrypted GitHub CLI backup; no token/passphrase in Git or argv.
Python stdlib + OpenSSL. Encrypt-then-MAC: AES-256-CBC/PBKDF2 and HMAC-SHA256.
"""
import argparse,getpass,hashlib,hmac,json,os,secrets,subprocess
from pathlib import Path
MAGIC=b'TMCVAULT1';ITER=600000

def crypt(raw,password,decrypt=False):
 r,w=os.pipe()
 try:
  os.write(w,password.encode()+b'\n');os.close(w);w=-1
  p=subprocess.run(['openssl','enc','-aes-256-cbc','-pbkdf2','-iter',str(ITER),'-md','sha256','-d' if decrypt else '-e','-pass','fd:'+str(r)],input=raw,capture_output=True,pass_fds=(r,))
  if p.returncode:raise ValueError('Impossibile cifrare/decifrare archivio')
  return p.stdout
 finally:
  os.close(r)
  if w>=0:os.close(w)
def seal(raw,password):
 salt=secrets.token_bytes(16);body=MAGIC+salt+crypt(raw,password)
 key=hashlib.pbkdf2_hmac('sha256',password.encode(),salt+b'tmc-vault-mac',ITER)
 return body+hmac.digest(key,body,'sha256')
def unseal(raw,password):
 if len(raw)<89 or not raw.startswith(MAGIC):raise ValueError('Archivio non valido')
 body,tag=raw[:-32],raw[-32:];salt=body[len(MAGIC):len(MAGIC)+16]
 key=hashlib.pbkdf2_hmac('sha256',password.encode(),salt+b'tmc-vault-mac',ITER)
 if not hmac.compare_digest(hmac.digest(key,body,'sha256'),tag):raise ValueError('Chiave errata o archivio alterato')
 return crypt(body[len(MAGIC)+16:],password,True)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=['backup','restore']);p.add_argument('--vault',default='/home/user/private-backup/github-access.enc');p.add_argument('--config',default='/var/tmp/tmc-github-auth');p.add_argument('--key-file',help='Solo file esterno al workspace, mai incluso nel backup');a=p.parse_args()
 if a.key_file:
  keyfile=Path(a.key_file).resolve()
  if keyfile.is_relative_to('/home/user'):raise SystemExit('La chiave deve restare fuori dal workspace')
  password=keyfile.read_text().strip()
 else:password=getpass.getpass('Chiave archivio: ')
 if len(password)<16:raise SystemExit('Chiave troppo corta (minimo 16 caratteri)')
 v=Path(a.vault);config=Path(a.config).resolve()
 if a.action=='backup':
  files={f.name:f.read_text() for f in config.glob('*.yml')};assert 'hosts.yml' in files
  data=json.dumps(files).encode();blob=seal(data,password);assert unseal(blob,password)==data
  for damaged,wrong in [(blob[:-1]+bytes([blob[-1]^1]),password),(blob,password+'x')]:
   try:unseal(damaged,wrong)
   except ValueError:pass
   else:raise AssertionError('Authentication failed open')
  v.parent.mkdir(parents=True,exist_ok=True);v.parent.chmod(0o700);v.write_bytes(blob);v.chmod(0o600)
  print('Archivio cifrato creato; round trip, chiave errata e alterazione verificati.')
 else:
  if config.is_relative_to('/home/user'):raise SystemExit('Ripristinare le credenziali solo fuori dal workspace')
  files=json.loads(unseal(v.read_bytes(),password));assert isinstance(files,dict) and 'hosts.yml' in files
  config.mkdir(parents=True,exist_ok=True);config.chmod(0o700)
  for name,data in files.items():
   if name not in ['hosts.yml','config.yml'] or not isinstance(data,str):raise ValueError('Contenuto archivio non ammesso')
   f=config/name;f.write_text(data);f.chmod(0o600)
  print('Accesso CLI ripristinato nella directory esterna; nessun token stampato.')
if __name__=='__main__':main()
