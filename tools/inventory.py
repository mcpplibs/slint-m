#!/usr/bin/env python3
"""The public C++ surface of Slint's SDK headers, read by clang.

Shared by tools/gen_exports.py (which writes the module units) and
tools/check_exports.py (which audits them). The headers are those of the SDK
compat.slint binds (xim:slint, built from the release tag): the pinned
payload is downloaded once into target/upstream and checked against
upstream/documents.json, or `--sdk <dir>` names an extracted SDK.

`clang++ -Xclang -ast-dump=json -Xclang -ast-dump-filter=slint` over
slint.h, slint-interpreter.h and slint-platform.h; the declarations directly
in each PUBLIC namespace are the surface:

  slint                  the runtime API (and slint::language, the types of
                         the language's built-in elements generated code uses)
  slint::interpreter     loading `.slint` files at run time
  slint::platform        the platform API (a window adapter, a renderer)
  slint::platform::key_codes

`slint::private_api` and `slint::cbindgen_private` are the implementation
generated code and the inline functions reach; they are reachable through the
public types and exported by nothing. Out-of-class member definitions at
namespace scope are not declarations of their own.
"""
from dataclasses import dataclass, field
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

PUBLIC = ('slint', 'slint::language', 'slint::interpreter', 'slint::platform', 'slint::platform::key_codes')
KINDS = {'CXXRecordDecl', 'ClassTemplateDecl', 'EnumDecl', 'FunctionDecl', 'FunctionTemplateDecl',
         'TypeAliasDecl', 'TypeAliasTemplateDecl', 'TypedefDecl', 'VarDecl', 'UsingDecl'}
HEADERS = ('slint.h', 'slint-interpreter.h', 'slint-platform.h')


def fetch(root: Path, sdk: str | None = None) -> Path:
    """The SDK root (bin/, lib/, include/slint/)."""
    if sdk:
        return Path(sdk)
    pins = json.loads((root / 'upstream/documents.json').read_text(encoding='utf-8'))['sdk']
    cache = root / 'target/upstream'
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / Path(pins['url']).name
    if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != pins['sha256']:
        print('+ fetch', pins['url'], flush=True)
        with urllib.request.urlopen(pins['url'], timeout=300) as response:
            archive.write_bytes(response.read())
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != pins['sha256']:
        sys.exit(f'SDK archive checksum {digest} != {pins["sha256"]}')
    with tarfile.open(archive) as tar:
        top = tar.getmembers()[0].name.split('/')[0]
        if not (cache / top / 'include/slint/slint.h').exists():
            tar.extractall(cache, filter='data')
    return cache / top


def find_clang(explicit: str | None = None) -> str:
    for candidate in (explicit, os.environ.get('CLANGXX'), shutil.which('clang++')):
        if candidate:
            return candidate
    for home in [Path(os.environ[v]) for v in ('MCPP_HOME', 'XLINGS_HOME') if os.environ.get(v)] + \
                [Path.home() / '.mcpp', Path.home() / '.xlings']:
        found = sorted(home.glob('**/xim-x-llvm/*/bin/clang++*'))
        if found:
            return str(found[-1])
    sys.exit("clang++ not found: pass --clang or set CLANGXX (the audit reads clang's AST)")


@dataclass
class Surface:
    names: dict[str, list[str]] = field(default_factory=dict)   # namespace -> declared names
    features: list[str] = field(default_factory=list)            # SLINT_FEATURE_* the headers define
    feature_universe: list[str] = field(default_factory=list)    # every SLINT_FEATURE_* the headers test


def surface(sdk: Path, clang: str) -> Surface:
    include = sdk / 'include/slint'
    with tempfile.TemporaryDirectory() as tmp:
        tu = Path(tmp) / 'surface.cc'
        tu.write_text(''.join(f'#include <{h}>\n' for h in HEADERS))
        out = subprocess.run([clang, '-std=c++23', '-fsyntax-only', '-I', str(include),
                              '-Xclang', '-ast-dump=json', '-Xclang', '-ast-dump-filter=slint', str(tu)],
                             capture_output=True, text=True, check=True).stdout
    decoder = json.JSONDecoder()
    objects, i = [], 0
    while (j := out.find('{', i)) >= 0:
        obj, i = decoder.raw_decode(out, j)
        objects.append(obj)
    nested = set()

    def mark(node):
        for child in node.get('inner') or []:
            nested.add(child.get('id'))
            mark(child)
    for o in objects:
        mark(o)
    s = Surface({ns: [] for ns in PUBLIC})

    def visit(node, prefix):
        for child in node.get('inner') or []:
            kind = child['kind']
            if kind == 'NamespaceDecl':
                visit(child, f"{prefix}::{child.get('name', '')}")
                continue
            if prefix not in s.names or kind not in KINDS or child.get('isImplicit'):
                continue
            name = child.get('name')
            if not name:
                continue
            name = name.split('::')[-1]               # `using cbindgen_private::TimerMode;`
            if name.startswith('~') or name.startswith('operator'):
                if not name.startswith('operator'):
                    continue
            s.names[prefix].append(name)
    for o in objects:
        if o.get('id') not in nested and o['kind'] == 'NamespaceDecl' and o.get('name') == 'slint':
            visit(o, 'slint')
    for ns in s.names:
        s.names[ns] = sorted(set(s.names[ns]))
    generated = (include / 'private/slint_generated_public.h').read_text(encoding='utf-8')
    s.features = sorted(set(re.findall(r'#define (SLINT_FEATURE_\w+)', generated)))
    tested = set()
    for h in include.rglob('*.h'):
        tested |= set(re.findall(r'\b(SLINT_FEATURE_[A-Z0-9_]+)', h.read_text(encoding='utf-8')))
    s.feature_universe = sorted(tested | set(s.features))
    return s


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clang')
    parser.add_argument('--sdk')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    sf = surface(fetch(root, args.sdk), find_clang(args.clang))
    for ns, names in sf.names.items():
        print(f'{ns}: {len(names)}: {" ".join(names[:60])}')
    print('features defined:', sf.features)
    print('features tested:', sf.feature_universe)
