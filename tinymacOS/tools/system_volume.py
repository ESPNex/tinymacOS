#!/usr/bin/env python3
"""Pack the boot-read namespace and convert freestanding linked segments to real static Mach-O 64 executables."""
import hashlib,os,plistlib,struct,sys
from pathlib import Path
D=Path(sys.argv[1]);mode=sys.argv[2]
if mode=='cache':
    elf=(D/'kernel.elf').read_bytes();(D/'KernelCache').write_bytes(struct.pack('<8sIIQQ32s',b'TMKCACHE',2,62,64,len(elf),hashlib.sha256(elf).digest())+elf);sys.exit(0)
def macho(p):
    e=p.read_bytes();entry,po=struct.unpack_from('<QQ',e,24);psz,pn=struct.unpack_from('<HH',e,54);segs=[];body=bytearray(4096)
    for i in range(pn):
        t,fl,o,a,_,fs,ms,_=struct.unpack_from('<IIQQQQQQ',e,po+psz*i)
        if t!=1 or not ms:continue
        off=(len(body)+4095)&~4095;body.extend(bytes(off-len(body)));body.extend(e[o:o+fs]);prot=((fl&4)>>2)|(fl&2)|((fl&1)<<2);name='__TEXT' if prot&4 else '__DATA' if prot&2 else '__DATA_CONST'
        if name=='__TEXT':assert a==0x8000001000;ep=off+entry-a;a-=4096;ms+=4096;fs+=4096;off=0
        segs.append(struct.pack('<II16sQQQQiiII',0x19,72,name.encode(),a,(ms+4095)&~4095,off,fs,prot,prot,0,0))
    cmds=b''.join(segs)+struct.pack('<IIQQ',0x80000028,24,ep,0);head=struct.pack('<8I',0xfeedfacf,0x1000007,3,2,len(segs)+1,len(cmds),1,0)+cmds;body[:len(head)]=head;return bytes(body)
apps=[('finder','Finder','/System/Library/CoreServices'),('terminal','Terminal','/Applications/Utilities'),('textedit','TextEdit','/Applications'),('settings','System Settings','/Applications'),('about','About This Computer','/System/Library/CoreServices'),('calculator','Calculator','/Applications'),('calendar','Calendar','/Applications'),('monitor','Activity Monitor','/Applications/Utilities'),('gallery','Gallery','/Applications'),('diskutility','Disk Utility','/Applications/Utilities'),('notes','Notes','/Applications'),('stickies','Stickies','/Applications'),('timemachine','Time Machine','/Applications')]
if mode=='live':apps+=[('installer','Install tinymacOS','/Applications')]
files={};paths=['/'];entries=[(-1,1,'',None)]
def add(p,data):
    if p in paths:return paths.index(p)
    pa,name=p.rsplit('/',1);parent=add(pa or '/',None);id=len(paths);paths.append(p);entries.append((parent,int(data is None),name,data));return id
for x in ['/System','/Applications','/bin','/usr','/sbin']:add(x,None)
for src,name,loc in apps:
    base=loc+'/'+name+'.app/Contents';add(base+'/Info.plist',plistlib.dumps({'CFBundleExecutable':name,'CFBundleName':name,'CFBundleIdentifier':'org.tinymacos.'+src,'CFBundlePackageType':'APPL','CFBundleShortVersionString':'10.1.9','CFBundleVersion':'30B10','CFBundleIconFile':'AppIcon.png'}));add(base+'/MacOS/'+name,macho(D/(src+'.elf')))
for svg in sorted(Path('assets/control-center').glob('*.svg')):add('/System/Library/CoreServices/ControlCenter/Symbols/'+svg.name,svg.read_bytes())
add('/System/Library/CoreServices/ControlCenter/Symbols/manifest.json',(D/'control-svg.json').read_bytes())
add('/System/Library/CoreServices/WindowServer',macho(D/'desktop.elf'));add('/System/Library/CoreServices/SystemVersion.plist',plistlib.dumps({'ProductName':'tinymacOS','ProductVersion':'10.1.9','ProductBuildVersion':'30B10'}));add('/System/Library/KernelCollections',None)
add('/System/Library/Fonts/Interface.tfn',Path(os.environ.get('TMC_FONT','assets/type.tfn')).read_bytes())
# Original supplied PNGs and size-specific PNGs remain ordinary read-only files.
import json
atlas=json.loads((D/'icon-atlas.json').read_text());slots={int(k):v for k,v in atlas['slots'].items()}
for slot,name in slots.items():
    source=D/name if slot==38 else Path('assets/icons')/name
    add('/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/'+name,source.read_bytes())
appslots={'finder':1,'terminal':2,'textedit':3,'settings':4,'about':5,'calculator':16,'calendar':17,'monitor':18,'gallery':19,'installer':38,'diskutility':39,'notes':40,'stickies':41,'timemachine':42}
for src,name,loc in apps:
    icon=slots[appslots[src]];source=D/icon if src=='installer' else Path('assets/icons')/icon
    resources=loc+'/'+name+'.app/Contents/Resources/'
    add(resources+'AppIcon.png',source.read_bytes())
    for size in atlas['sizes']:add(resources+f'Icon{size}.png',(D/'icon-png'/icon.removesuffix('.png')/f'Icon{size}.png').read_bytes())
add('/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/IconsManifest.json',Path('assets/icons/manifest.json').read_bytes())
add('/System/Library/Fonts/Display-Semibold-OFL.txt',Path('assets/font-source/OFL.txt').read_bytes())

# No browser resources in this baseline image. WebKit is developed separately.
for name,src in zip(('Prisma','Alba','Laguna','Aurora','Prisma glaciale','Aurora boreale','Rame vespertino','Riviera','Levante','Meriggio','Notturno','Brezza','Tramontana'),('prisma','alba','laguna','aurora','glaciale','boreale','rame','riviera','levante','meriggio','notturno','brezza','tramontana')):add('/System/Library/Desktop Pictures/'+name+'.qoi',Path('assets/'+src+'.qoi').read_bytes())
commands=(Path('include/commands.def').read_text().splitlines());tool=macho(D/'command.elf')
for path in commands:
    if path:add(path,tool)
# Driver kernel extensions: binary-plist Info + raw ELF ET_REL executable.
for k in ('e1000', 'wifi'):
    b = '/System/Library/Extensions/%s.kext/Contents' % k
    add(b + '/Info.plist', plistlib.dumps({'CFBundleIdentifier': 'org.tinymacos.driver.' + k,
        'CFBundleExecutable': k, 'CFBundlePackageType': 'BNDL', 'CFBundleVersion': '1.0',
        'CFBundleName': k, 'OSBundleLibraries': {'org.tinymacos.kernel': '1.0'}},
        fmt=plistlib.FMT_BINARY))
    add(b + '/MacOS/' + k, (D / ('kext-' + k + '.o')).read_bytes())
# Identical executable extents are stored once. Each path is exec'd with its own argv and page tables.
offs={};data=bytearray();table=bytearray();start=16+64*len(entries)
for parent,isdir,name,obj in entries:
    if obj is not None and obj not in offs:offs[obj]=start+len(data);data+=obj
    table+=struct.pack('<iIII48s',parent,isdir,0 if obj is None else offs[obj],0 if obj is None else len(obj),name.encode())
raw=struct.pack('<8sII',b'TMVOL2\0\0',len(entries),64)+table+data
(D/('System-'+mode+'.vol')).write_bytes(raw+hashlib.sha256(raw).digest())
