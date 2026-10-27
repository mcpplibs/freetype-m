#!/usr/bin/env python3
"""The public surface of FreeType 2.13.3, from the pinned release's headers.

Shared by tools/gen_exports.py (which writes the export list of
src/freetype.cppm and the constants of src/macros.cppm) and
tools/check_exports.py (which audits the modules against it). The headers
come from the upstream tarball, downloaded once into target/upstream and
checked against upstream/documents.json: the same bytes compat.freetype
builds.

The public headers are include/freetype/*.h, every one <ft2build.h> names
(FT_FREETYPE_H, FT_GLYPH_H, FT_CACHE_H …) plus ftlogging.h, except:
"""
from dataclasses import dataclass, field
import hashlib
import json
from pathlib import Path
import re
import sys
import tarfile
import urllib.request

# Headers of include/freetype/ that are not part of the module, with the reason.
NOT_HEADERS = {
    'ftmac.h': 'the Classic Mac OS / Carbon API: its declarations use Carbon types (Handle, FSSpec, FSRef) '
               'and src/base/ftmac.c builds them only under FT_MACINTOSH, which no supported target defines',
    'ftchapters.h': 'documentation chapters only, no declarations',
    'fterrdef.h': 'the error table read by fterrors.h (an X-macro list): its entries are the FT_Err_* constants',
}

__doc__ += ''.join(f'\n  - {k}: {v}' for k, v in NOT_HEADERS.items()) + '\n'


def fetch(root: Path) -> Path:
    pins = json.loads((root / 'upstream/documents.json').read_text(encoding='utf-8'))
    cache = root / 'target/upstream'
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / 'freetype-release.tar.gz'
    if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != pins['upstream_sha256']:
        print('+ fetch', pins['upstream'], flush=True)
        with urllib.request.urlopen(pins['upstream'], timeout=180) as response:
            archive.write_bytes(response.read())
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != pins['upstream_sha256']:
        sys.exit(f'upstream archive checksum {digest} != {pins["upstream_sha256"]}')
    with tarfile.open(archive) as tar:
        top = tar.getmembers()[0].name.split('/')[0]
        if not (cache / top / 'include/freetype/freetype.h').exists():
            tar.extractall(cache, filter='data')
    return cache / top


@dataclass
class Macro:
    name: str
    header: str           # relative to include/
    params: str | None    # None: object-like
    body: str             # as written, continuations joined, comments removed


@dataclass
class Surface:
    headers: list[str] = field(default_factory=list)         # relative to include/
    functions: list[str] = field(default_factory=list)       # FT_EXPORT declarations
    types: list[str] = field(default_factory=list)           # typedef names
    tags: list[str] = field(default_factory=list)            # struct / union / enum tags
    enumerators: list[str] = field(default_factory=list)     # of the named enums
    error_constants: list[str] = field(default_factory=list) # FT_Err_* and FT_Mod_Err_*: unnamed enums
    macros: dict[str, Macro] = field(default_factory=dict)   # first definition of each name
    redefined: set[str] = field(default_factory=set)         # names #define'd more than once


def strip_comments(text: str) -> str:
    text = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def strip_preprocessor(text: str) -> str:
    return re.sub(r'^[ \t]*#(?:\\\n|[^\n])*', '', text, flags=re.M)


def without_bodies(text: str) -> str:
    """The text with every {…} replaced by {}."""
    out, depth = [], 0
    for ch in text:
        if ch == '{':
            if depth == 0:
                out.append('{}')
            depth += 1
        elif ch == '}':
            depth -= 1
        elif depth == 0:
            out.append(ch)
    return ''.join(out)


def statements(text: str):
    """File-scope statements, split at ';' outside braces."""
    current, depth = [], 0
    for ch in text:
        current.append(ch)
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
        elif ch == ';' and depth == 0:
            yield ' '.join(''.join(current).split())
            current = []


def split_top(text: str, sep: str = ','):
    parts, current, depth = [], [], 0
    for ch in text:
        if ch in '([{':
            depth += 1
        elif ch in ')]}':
            depth -= 1
        if ch == sep and depth == 0:
            parts.append(''.join(current))
            current = []
        else:
            current.append(ch)
    parts.append(''.join(current))
    return [p.strip() for p in parts if p.strip()]


def enumerators(body: str) -> list[str]:
    names = []
    for entry in split_top(body):
        m = re.match(r'(?:FT_ENC_TAG|FT_IMAGE_TAG)\s*\(\s*(\w+)', entry) or re.match(r'(\w+)', entry)
        names.append(m.group(1))
    return names


def public_headers(include: Path) -> list[str]:
    # config/integer-types.h declares FT_Int16, FT_Int32, FT_Int64 … (the
    # other config headers declare macros only).
    return ['freetype/config/integer-types.h'] + sorted(
        f'freetype/{p.name}' for p in (include / 'freetype').glob('*.h') if p.name not in NOT_HEADERS)


def macro_headers(include: Path) -> list[str]:
    """Every header whose macros a translation unit including the public ones sees."""
    config = sorted(f'freetype/config/{p.name}' for p in (include / 'freetype/config').glob('*.h')
                    if p.name not in ('ftmodule.h', 'integer-types.h'))
    return ['ft2build.h'] + config + public_headers(include) + ['freetype/fterrdef.h']


def surface(upstream: Path) -> Surface:
    include = upstream / 'include'
    s = Surface(headers=public_headers(include))
    functions, types, tags, enums = set(), set(), set(), []
    for header in s.headers:
        text = strip_preprocessor(strip_comments((include / header).read_text(encoding='utf-8')))
        # FT_EXPORT( type ) name( … ): normalise the annotation away before
        # reading declarations; the declared name precedes the parameters.
        functions |= set(re.findall(r'\bFT_EXPORT\(\s*[^()]*?\s*\)\s*(\w+)\s*\(', text))
        text = re.sub(r'\bFT_EXPORT\(\s*([^()]*?)\s*\)', r'\1', text)
        text = re.sub(r'\b(FT_BEGIN_HEADER|FT_END_HEADER)\b', '', text)
        for st in statements(text):
            head = without_bodies(st)
            m = re.match(r'(?:typedef\s+)?(?:const\s+)?(struct|union|enum)\s+(\w+)', head)
            if m:
                tags.add(m.group(2))
            if head.startswith('typedef'):
                fp = re.search(r'\(\s*\*\s*(\w+)\s*\)\s*\(', head)
                if fp:
                    types.add(fp.group(1))
                else:
                    name = re.search(r'(\w+)\s*(?:\[[^\]]*\]\s*)*;$', head)
                    types.add(name.group(1))
            em = re.match(r'(?:typedef\s+)?enum\s*(\w*)\s*\{(.*)\}\s*(\w*)\s*;$', st, flags=re.S)
            if em:
                tag, body, alias = em.groups()
                if not tag and not alias:
                    sys.exit(f'{header}: an unnamed enum the inventory does not know')
                enums += enumerators(body)
    s.functions, s.types, s.tags, s.enumerators = sorted(functions), sorted(types), sorted(tags), sorted(set(enums))
    # The error codes: unnamed enums generated by fterrors.h / ftmoderr.h
    # from their X-macro lists.
    errdef = strip_comments((include / 'freetype/fterrdef.h').read_text(encoding='utf-8'))
    errors = ['FT_Err_' + n for n in re.findall(r'\bFT_(?:NO)?ERRORDEF_\(\s*(\w+)\s*,', errdef)] + ['FT_Err_Max']
    moderr = strip_comments((include / 'freetype/ftmoderr.h').read_text(encoding='utf-8'))
    moderr = moderr[moderr.index('FT_MODERR_START_LIST'):]
    errors += ['FT_Mod_Err_' + n for n in re.findall(r'^\s*FT_MODERRDEF\(\s*(\w+)\s*,', moderr, flags=re.M)] + ['FT_Mod_Err_Max']
    s.error_constants = errors
    for header in macro_headers(include):
        raw = strip_comments((include / header).read_text(encoding='utf-8'))
        for m in re.finditer(r'^[ \t]*#[ \t]*define[ \t]+(\w+)(\([^)]*\))?((?:[^\n\\]|\\\n|\\)*)', raw, flags=re.M):
            name = m.group(1)
            if name in s.macros:
                s.redefined.add(name)
                continue
            body = ' '.join(m.group(3).replace('\\\n', ' ').split())
            s.macros[name] = Macro(name, header, m.group(2), body)
    return s


if __name__ == '__main__':
    root = Path(__file__).resolve().parents[1]
    sf = surface(fetch(root))
    print(f'{len(sf.headers)} headers: {len(sf.functions)} functions, {len(sf.types)} types, {len(sf.tags)} tags, '
          f'{len(sf.enumerators)} enumerators, {len(sf.error_constants)} error constants, {len(sf.macros)} macros')
    if '-v' in sys.argv:
        print('functions:', ' '.join(sf.functions))
        print('types:', ' '.join(sf.types))
        print('tags:', ' '.join(sf.tags))
        print('enumerators:', ' '.join(sf.enumerators))
