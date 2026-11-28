#!/usr/bin/env python3
"""Build pinned decode-only zlib/bzip2/LZFSE/XZ, no libc or host syscalls.
This is a build step, not a test runner. Upstream files remain unchanged.
"""
from pathlib import Path
import hashlib, json, subprocess, sys
root = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1]).resolve()
if out.is_relative_to(root):
    raise SystemExit('Codec objects must be built outside the workspace')
work = out / 'image-codecs'
work.mkdir(parents=True, exist_ok=True)
common = ['gcc', '-O2', '-g0', '-ffreestanding', '-fno-pie', '-fno-builtin',
          '-fno-stack-protector', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables',
          '-mno-red-zone', '-mgeneral-regs-only', '-ffunction-sections', '-fdata-sections',
          '-fno-ident', '-ffile-prefix-map='+str(root)+'=.']
objects = []
for version, names, flags in [
    ('zlib-1.3.1', ['inflate.c', 'inftrees.c', 'inffast.c', 'adler32.c', 'crc32.c', 'zutil.c'], ['-DZ_SOLO', '-DZ_PREFIX']),
    ('xz-embedded', ['xz_dec_stream.c', 'xz_dec_lzma2.c', 'xz_dec_bcj.c', 'xz_crc32.c', 'xz_crc64.c', 'xz_sha256.c'], ['-DXZ_DEC_DYNALLOC', '-DXZ_USE_CRC64', '-DXZ_USE_SHA256', '-DXZ_DEC_X86', '-DXZ_DEC_ARM', '-DXZ_DEC_ARMTHUMB', '-DXZ_DEC_ARM64', '-DXZ_DEC_POWERPC', '-DXZ_DEC_IA64', '-DXZ_DEC_SPARC', '-DXZ_DEC_RISCV', '-Dmalloc=image_mem_alloc', '-Dfree=image_mem_free']),
    ('lzfse', ['lzfse_decode_base.c', 'lzfse_fse.c', 'lzvn_decode_base.c'], ['-DNDEBUG']),
    ('bzip2-1.0.8', ['decompress.c', 'huffman.c', 'randtable.c', 'crctable.c', 'bzlib.c'], ['-DBZ_NO_STDIO', '-Dmalloc=image_mem_alloc', '-Dfree=image_mem_free']),
]:
    source = root / 'vendor' / version
    lock = json.loads((source / 'ORIGIN.json').read_text())
    for name, expected in lock['source_files'].items():
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != expected:
            raise SystemExit('Changed pinned codec source: '+name)
    for name in names:
        obj = work / (version+'-'+name+'.o')
        subprocess.run(common+flags+['-I'+str(source), '-c', str(source/name), '-o', str(obj)], check=True)
        objects.append(str(obj))
subprocess.run(['ld', '-r', '--gc-sections', '-u', 'z_inflateInit_', '-u', 'z_inflate',
                '-u', 'z_inflateEnd', '-u', 'BZ2_bzDecompressInit', '-u', 'BZ2_bzDecompress',
                '-u', 'BZ2_bzDecompressEnd', '-u', 'lzfse_decode', '-u', 'xz_dec_init', '-u', 'xz_dec_run', '-u', 'xz_dec_end', '-u', 'xz_crc32_init', '-u', 'xz_crc64_init', *objects, '-o', str(out/'k-image-codecs.o')], check=True)

subprocess.run(['objcopy', '--strip-unneeded', str(out/'k-image-codecs.o')], check=True)
