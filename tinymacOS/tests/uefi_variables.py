"""Read-only EDK2 variable-store decoder used only in disposable guest QA."""
import struct
def vars_active(path):
    raw=path.read_bytes();out={}
    # EDK2 normal/authenticated variable headers; only committed VAR_ADDED.
    for at in range(len(raw)-60):
        if raw[at:at+3]!=b'\xaa\x55\x3f':continue
        for header,no,do,go in [(60,36,40,44),(32,8,12,16)]:
            ns,ds=struct.unpack_from('<I',raw,at+no)[0],struct.unpack_from('<I',raw,at+do)[0]
            if ns<4 or ns>1024 or ns%2 or ds>16384 or at+header+ns+ds>len(raw):continue
            name=raw[at+header:at+header+ns]
            if not name.endswith(b'\0\0'):continue
            try:name=name.decode('utf-16le').rstrip('\0')
            except UnicodeDecodeError:continue
            if not name or any(ord(c)<32 or ord(c)>126 for c in name):continue
            out[(raw[at+go:at+go+16],name)]=raw[at+header+ns:at+header+ns+ds]
    return out
