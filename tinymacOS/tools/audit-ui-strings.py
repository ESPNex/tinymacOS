#!/usr/bin/env python3
"""Deterministic static C literal inventory, not a runtime/UI completeness claim.

Includes client/compositor/shared headers, boot and disk/filesystem messages that
can be returned to the UI. Keeps identifiers and diagnostic literals as well:
classification is a review aid, never proof that a literal reaches the screen.
Comments, character constants and include directives are not UI strings.
"""
import argparse
import ast
import collections
import csv
import hashlib
import io
import json
from pathlib import Path
import re
import warnings

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'docs/audit-10.3'
TOKEN = re.compile(r'(?P<comment>//[^\n]*|/\*[\s\S]*?\*/)|'
                   r"(?P<char>'(?:\\[\s\S]|[^'\\\n])*')|"
                   r'(?P<string>(?:u8|[LuU])?"(?:\\[\s\S]|[^"\\])*")')
TECH = re.compile(r'\b(?:APFS|BTRFS|GOP|UEFI|enrollment|scheduler|crc32c|CoW|backend)\b', re.I)
SAFETY = re.compile(r'elimin|pers[ioe]|irreversibil|non recuper|non inizial|password|chiave|conferm|sola lettura|non salv|interruzion|non.*annulla', re.I)


def literals(source):
    for match in TOKEN.finditer(source):
        if not match.group('string'):
            continue
        line_start = source.rfind('\n', 0, match.start()) + 1
        line_end = source.find('\n', match.end())
        if line_end < 0:
            line_end = len(source)
        context = source[line_start:line_end].strip()
        if re.match(r'#\s*include\b', source[line_start:match.start()].strip()):
            continue
        raw = match.group('string')
        quoted = raw[raw.index('"'):]
        with warnings.catch_warnings():
            warnings.simplefilter('ignore', (DeprecationWarning, SyntaxWarning))
            value = ast.literal_eval(quoted)
        yield source.count('\n', 0, match.start()) + 1, raw, value, context


def render():
    files = sorted(p for d in ('userspace', 'desktop', 'include', 'boot', 'fs', 'storage')
                   for p in (ROOT / d).glob('*') if p.suffix in ('.c', '.h'))
    rows, manifest = [], {}
    for path in files:
        data = path.read_bytes()
        relative = path.relative_to(ROOT).as_posix()
        manifest[relative] = hashlib.sha256(data).hexdigest()
        for line, raw, text, context in literals(data.decode('utf-8')):
            tags = []
            if TECH.search(text):
                tags.append('technical-review')
            if SAFETY.search(text):
                tags.append('safety-retain-meaning')
            if re.search(r'\b(?:trace|log|panic|_Static_assert)\s*\(', context):
                tags.append('diagnostic-or-assert-hint')
            if not text.strip() or text.startswith('/') or re.fullmatch(r'[A-Za-z0-9_./%+*:=<>\-]+', text):
                tags.append('identifier-or-fragment-hint')
            rows.append((relative, line, '|'.join(tags) or 'text-review', text, raw))
    buffer = io.StringIO(newline='')
    writer = csv.writer(buffer, lineterminator='\n')
    writer.writerow(('file', 'line', 'review_tags', 'text_escaped', 'C_literal'))
    # Keep control bytes visible and one logical CSV record per source literal;
    # embedded CR/LF otherwise look like trailing whitespace in source review.
    writer.writerows((file, line, tags, json.dumps(text, ensure_ascii=False)[1:-1], raw)
                     for file, line, tags, text, raw in rows)
    summary = {
        'schema': 1,
        'scope': 'Static literals in userspace, desktop, shared include, boot, fs, storage C/H files; Safari excluded and untouched',
        'limitations': ['Not a C preprocessor or runtime call graph; inactive branches remain inventoried',
                       'Adjacent literals, dynamic text and macro substitutions require source/runtime review',
                       'Tags are hints, not automatic proof of visibility, safety or translation quality',
                       'Network/vendor/third-party messages outside these directories are not covered'],
        'source_files': manifest,
        'literal_count': len(rows),
        'counts_by_directory': dict(sorted(collections.Counter(r[0].split('/')[0] for r in rows).items())),
        'counts_by_tag': dict(sorted(collections.Counter(t for r in rows for t in r[2].split('|')).items())),
    }
    return {'ui-strings.csv': buffer.getvalue(), 'ui-strings-summary.json': json.dumps(summary, indent=2, ensure_ascii=False) + '\n'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true', help='Refresh committed inventory after reviewing the source diff')
    parser.add_argument('--check', action='store_true', help='Fail when sources or inventory changed (default)')
    args = parser.parse_args()
    if args.write and args.check:
        parser.error('choose --write or --check')
    generated = render()
    if args.write:
        OUTPUT.mkdir(parents=True, exist_ok=True)
        for name, content in generated.items():
            (OUTPUT / name).write_text(content, encoding='utf-8')
    else:
        for name, content in generated.items():
            path = OUTPUT / name
            if not path.exists() or path.read_bytes() != content.encode('utf-8'):
                raise SystemExit(f'STALE {path.relative_to(ROOT)}: review source changes then run --write')
    summary = json.loads(generated['ui-strings-summary.json'])
    print(f"PASS static UI inventory: {len(summary['source_files'])} files, {summary['literal_count']} literals; not a visual acceptance claim")


if __name__ == '__main__':
    main()
