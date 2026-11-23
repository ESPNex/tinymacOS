#!/usr/bin/env python3
"""Derive a single-module build platform from OpenCorePkg 1.0.8's DSC.
Uses its real CPU timer, entry point, debug, and memory libraries. It does
not compile or replace OpenCore. All original copyright/license text stays.
"""
from pathlib import Path
import hashlib, json, sys
v=Path(__file__).resolve().parent
udk=Path(sys.argv[1]).resolve()
p=udk/'OpenCorePkg/OpenCorePkg.dsc'
s=p.read_text()
expected=json.loads((v/'upstream-lock.json').read_text())['build']['opencore_dsc_sha256']
if hashlib.sha256(p.read_bytes()).hexdigest()!=expected:
    raise SystemExit('OpenCorePkg.dsc does not match pinned 1.0.8 sources')
start=s.index('[Components]');end=s.index('[LibraryClasses]',start)
s=s[:start]+'[Components]\n  MdeModulePkg/Bus/Isa/Ps2KeyboardDxe/Ps2KeyboardDxe.inf\n\n'+s[end:]
s=s.replace('  FLASH_DEFINITION        = OpenCorePkg/OpenCorePkg.fdf','  OUTPUT_DIRECTORY        = Build/TmcPs2')
s=s.replace('  PLATFORM_NAME           = OpenCorePkg','  PLATFORM_NAME           = TmcPs2')
s=s.replace('!include NetworkPkg/NetworkDefines.dsc.inc','')
start=s.index('[PcdsFixedAtBuild]');end=s.index('[BuildOptions]',start)
s=s[:start]+s[end:]
(p.parent/'TmcPs2.dsc').write_text(s)
print(p.parent/'TmcPs2.dsc')
