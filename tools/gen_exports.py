#!/usr/bin/env python3
"""Write the generated module surface of FreeType 2.13.3 from its headers.

- src/freetype.cppm exports, by `export using`, every type, struct / union /
  enum tag, enumerator of a named enum and FT_EXPORT function of the public
  headers (tools/inventory.py): the upstream globals, unchanged.
- src/macros_constants.cppm (partition :constants of the macros module) maps
  every constant macro to an `inline constexpr` of its macro's type, every
  macro naming a type to an alias, and the FT_Err_* / FT_Mod_Err_* error codes
  (enumerators of unnamed enums, which are TU-local and cannot be exported as
  they are) to FT_Error constants, in mcpplibs::mwrap::freetype::macros.
- include/mwrap/freetype/macros.hpp is the side header: every public header.
- tests/exports_test.cpp names every exported entity through the import
  alone: each type and tag, enumerator, constant, alias and mapped function,
  and takes the address of every function, so each must also link.

The macros that are functions (FT_HAS_KERNING, FT_LOAD_TARGET_, FT_MAKE_TAG
…) are written by hand in src/macros_functions.cppm; the classification
below accounts for every other macro, and tools/check_exports.py fails on
any it does not know.

With --check, nothing is written and a difference fails.
"""
import argparse
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from inventory import fetch, surface  # noqa: E402

# Function-like macros mapped to functions in src/macros_functions.cppm.
FUNCTIONS = [
    'FT_HAS_HORIZONTAL', 'FT_HAS_VERTICAL', 'FT_HAS_KERNING', 'FT_IS_SCALABLE', 'FT_IS_SFNT', 'FT_IS_FIXED_WIDTH',
    'FT_HAS_FIXED_SIZES', 'FT_HAS_FAST_GLYPHS', 'FT_HAS_GLYPH_NAMES', 'FT_HAS_MULTIPLE_MASTERS',
    'FT_IS_NAMED_INSTANCE', 'FT_IS_VARIATION', 'FT_IS_CID_KEYED', 'FT_IS_TRICKY', 'FT_HAS_COLOR', 'FT_HAS_SVG',
    'FT_HAS_SBIX', 'FT_HAS_SBIX_OVERLAY', 'FT_LOAD_TARGET_', 'FT_LOAD_TARGET_MODE', 'FT_MAKE_TAG', 'FT_IS_EMPTY',
    'FT_BOOL', 'FT_ERROR_BASE', 'FT_ERROR_MODULE', 'FT_CURVE_TAG', 'FTC_IMAGE_TYPE_COMPARE', 'FT_FACE_DRIVER_NAME',
]

# Macros that stay macros, in <mwrap/freetype/macros.hpp>, with the reason a
# constant or a function cannot be them. (Include names, include guards and
# empty #ifdef switches are classified by rule, in classify().)
SIDE_HEADER = {
    'FT_ENC_TAG': 'expands to an enumerator definition (`value = …`) inside an enum',
    'FT_IMAGE_TAG': 'expands to an enumerator definition (`value = …`) inside an enum',
    'FT_ERR_XCAT': 'pastes tokens',
    'FT_ERR_CAT': 'pastes tokens',
    'FT_ERR': 'pastes its argument into a name: FT_ERR( Ok ) is FT_Err_Ok',
    'FT_ERR_EQ': 'pastes its error name into FT_Err_<name>',
    'FT_ERR_NEQ': 'pastes its error name into FT_Err_<name>',
    'FT_VALIDATE_GX_BITFIELD': 'pastes its argument into FT_VALIDATE_<tag>_INDEX',
    'FT_ERR_PREFIX': 'the X-macro machinery of fterrors.h, redefined to generate error tables',
    'FT_ERR_BASE': 'the X-macro machinery of fterrors.h, redefined to generate error tables',
    'FT_ERRORDEF': 'the X-macro machinery of fterrors.h, redefined to generate error tables',
    'FT_ERRORDEF_': 'the X-macro machinery of fterrors.h, redefined to generate error tables',
    'FT_NOERRORDEF_': 'the X-macro machinery of fterrors.h, redefined to generate error tables',
    'FT_ERROR_START_LIST': 'the X-macro machinery of fterrors.h, redefined to generate error tables',
    'FT_ERROR_END_LIST': 'the X-macro machinery of fterrors.h, redefined to generate error tables',
    'FT_MODERRDEF': 'the X-macro machinery of ftmoderr.h, redefined to generate tables',
    'FT_MODERR_START_LIST': 'the X-macro machinery of ftmoderr.h, redefined to generate tables',
    'FT_MODERR_END_LIST': 'the X-macro machinery of ftmoderr.h, redefined to generate tables',
    'FT_UNUSED': 'assigns any lvalue to itself, to silence an unused-variable warning',
    'FT_STATIC_CAST': 'takes a type as its argument',
    'FT_REINTERPRET_CAST': 'takes a type as its argument',
    'FT_STATIC_BYTE_CAST': 'takes a type as its argument',
    'FT_MACINTOSH': 'a switch for #ifdef, defined only by Classic Mac OS compilers',
}

# Macros not part of the API a program uses, with the reason.
EXCLUDED = {
    'FT_EXPORT': 'a linkage annotation of declarations',
    'FT_PUBLIC_FUNCTION_ATTRIBUTE': 'a linkage annotation of declarations',
    'FT_BEGIN_HEADER': 'opens the extern "C" block of a header',
    'FT_END_HEADER': 'closes the extern "C" block of a header',
}
STDLIB_REASON = ("the C library mapping of FreeType's implementation (config/ftstdlib.h): "
                 "ft_memcpy is memcpy, FT_FILE is FILE …; use the standard library")

# Macros naming a type: aliases (captured, so each is its platform's type).
TYPE_MACROS = {'FT_INT64', 'FT_UINT64'}


def classify(sf):
    """name -> (kind, reason): kind is constant, type, function, error, side, excluded."""
    known_types = set(sf.types) | set(sf.tags)
    out = {}
    for name, m in sf.macros.items():
        body = m.body
        if name in FUNCTIONS:
            out[name] = ('function', 'src/macros_functions.cppm')
        elif name in SIDE_HEADER:
            out[name] = ('side', SIDE_HEADER[name])
        elif name in EXCLUDED:
            out[name] = ('excluded', EXCLUDED[name])
        elif m.header == 'freetype/config/ftheader.h' and (body.startswith('<') or re.fullmatch(r'FT_\w+_H', body)):
            out[name] = ('side', 'an #include operand (`#include FT_FREETYPE_H`)')
        elif not body and re.search(r'_H_$|_H__$|^__FTERRORS_H__$', name):
            out[name] = ('excluded', 'include guard')
        elif not body:
            out[name] = ('side', 'a switch for #ifdef, defined empty')
        elif m.header == 'freetype/config/ftstdlib.h' and not re.fullmatch(r'FT_[A-Z_]+_(MAX|MIN|BIT)', name):
            out[name] = ('excluded', STDLIB_REASON)
        elif m.params is not None:
            out[name] = ('unclassified', 'a function-like macro neither mapped nor listed')
        elif name in TYPE_MACROS or body in known_types:
            out[name] = ('type', 'an alias of ' + body)
        else:
            out[name] = ('constant', body)
    return out


def includes(sf):
    """The #include lines of every public header, <freetype/freetype.h> first."""
    names = ['ft2build.h', 'freetype/freetype.h'] + [h for h in sf.headers if h not in (
        'freetype/freetype.h', 'freetype/config/integer-types.h')]
    return [f'#include <{h}>' for h in names]


def render_root(sf):
    clash = set(sf.macros) & (set(sf.types) | set(sf.tags) | set(sf.enumerators) | set(sf.functions))
    if clash:
        sys.exit('names that are also macros (an `export using` would expand them): ' + ', '.join(sorted(clash)))
    tags = [t for t in sf.tags if t not in sf.types]
    out = ['// Generated by tools/gen_exports.py from the headers of FreeType 2.13.3 — do',
           '// not edit by hand.',
           '//',
           '// mcpplibs.mwrap.freetype — FreeType 2, the font engine, as a C++ module.',
           '// Every exported name is the upstream global of the public headers',
           '// (<freetype/freetype.h>, ftglyph.h, ftoutln.h, ftstroke.h, ftcache.h …),',
           '// unchanged; the root also re-exports the macro mappings. The library is',
           '// compat.freetype, built from the same release.',
           'module;', *includes(sf), '',
           'export module mcpplibs.mwrap.freetype;', '',
           'export import mcpplibs.mwrap.freetype.macros;', '',
           f'// types ({len(sf.types)})']
    out += [f'export using ::{n};' for n in sf.types]
    out.append(f'\n// struct, union and enum tags ({len(tags)})')
    out += [f'export using ::{n};' for n in tags]
    out.append(f'\n// enumerators of the named enums ({len(sf.enumerators)})')
    out += [f'export using ::{n};' for n in sf.enumerators]
    out.append(f'\n// functions ({len(sf.functions)})')
    out += [f'export using ::{n};' for n in sf.functions]
    return '\n'.join(out) + '\n'


def render_constants(sf, kinds):
    constants = [n for n, (k, _) in kinds.items() if k == 'constant']
    types = [n for n, (k, _) in kinds.items() if k == 'type']
    out = ['// Generated by tools/gen_exports.py from the headers of FreeType 2.13.3 — do',
           '// not edit by hand.',
           '//',
           '// The constant macros of FreeType as `inline constexpr`, each of its macro\'s',
           '// type (FT_LOAD_*, FT_FACE_FLAG_*, FT_STYLE_FLAG_*, FT_OUTLINE_*,',
           '// FT_CURVE_TAG_*, TTAG_*, TT_*, FT_PARAM_TAG_*, FT_VALIDATE_*, the legacy',
           '// ft_* spellings …), the macros naming a type as aliases, and the FT_Err_* /',
           '// FT_Mod_Err_* codes (enumerators of unnamed enums: TU-local, not exportable',
           '// as they are) as FT_Error constants. Each value is captured in the global',
           '// module fragment, the macro set aside, and the constant declared under the',
           '// macro\'s name in mcpplibs::mwrap::freetype::macros.',
           'module;', *includes(sf), '',
           'namespace mwrap_freetype_capture {']
    out += [f'inline constexpr auto value_{n} = {n};' for n in constants]
    out += [f'using type_{n} = {n};' for n in types]
    out.append('}  // namespace mwrap_freetype_capture\n')
    out += [f'#undef {n}' for n in constants + types]
    out += ['', 'export module mcpplibs.mwrap.freetype.macros:constants;', '',
            'export namespace mcpplibs::mwrap::freetype::macros {', '',
            f'// constants ({len(constants)})']
    out += [f'inline constexpr auto {n} = mwrap_freetype_capture::value_{n};' for n in constants]
    out.append(f'\n// types ({len(types)})')
    out += [f'using {n} = mwrap_freetype_capture::type_{n};' for n in types]
    out.append(f'\n// error codes ({len(sf.error_constants)})')
    out += [f'inline constexpr FT_Error {n} = ::{n};' for n in sf.error_constants]
    out.append('\n}  // namespace mcpplibs::mwrap::freetype::macros')
    return '\n'.join(out) + '\n'


def render_side_header(sf, kinds):
    side = sorted(n for n, (k, _) in kinds.items() if k == 'side')
    out = ['// Generated by tools/gen_exports.py — do not edit by hand.',
           '//',
           "// <mwrap/freetype/macros.hpp> — FreeType's macros as macros, for the ones",
           '// a constant or a function cannot be: the include names (FT_FREETYPE_H,',
           '// FT_GLYPH_H …), the enumerator builders FT_ENC_TAG / FT_IMAGE_TAG, the',
           '// token-pasting FT_ERR / FT_ERR_EQ / FT_ERR_NEQ / FT_VALIDATE_GX_BITFIELD,',
           '// the X-macro machinery of fterrors.h (FT_ERRORDEF, FT_ERROR_START_LIST …,',
           '// to generate error tables), FT_UNUSED, the cast macros taking a type and',
           '// the configuration switches for #ifdef (FT_CONFIG_OPTION_* …): '
           f'{len(side)} in all.',
           '//',
           '// Include it before `import mcpplibs.mwrap.freetype;`: it is the public',
           '// headers, so every other macro is defined too, and while a macro is',
           '// defined it expands even inside `mcpplibs::mwrap::freetype::macros::NAME`.',
           '#pragma once', *includes(sf), '']
    return '\n'.join(out)


def render_exports_test(sf, kinds):
    constants = [n for n, (k, _) in kinds.items() if k == 'constant']
    types = [n for n, (k, _) in kinds.items() if k == 'type']
    functions = [n for n, (k, _) in kinds.items() if k == 'function']
    tags = [t for t in sf.tags if t not in sf.types]
    out = ['// Generated by tools/gen_exports.py — do not edit by hand.',
           '//',
           '// Every exported entity, named through `import mcpplibs.mwrap.freetype;`',
           '// alone: each type and tag, enumerator, constant, alias and mapped',
           '// function; the address of every function is taken, so each also links.',
           'import std;',
           'import mcpplibs.mwrap.freetype;', '',
           'namespace m = mcpplibs::mwrap::freetype::macros;', '',
           'namespace types {']
    out += [f'using t{i} = ::{n};' for i, n in enumerate(sf.types)]
    out += [f'using tag{i} = ::{n};' for i, n in enumerate(tags)]
    out += [f'using alias{i} = m::{n};' for i, n in enumerate(types)]
    out += ['}  // namespace types', '',
            f'[[maybe_unused]] constexpr long long enumerators[] = {{']
    out += [f'    static_cast<long long>(::{n}),' for n in sf.enumerators]
    out += ['};', '', 'constexpr long long constants[] = {']
    out += [f'    static_cast<long long>(m::{n}),' for n in constants]
    out += [f'    static_cast<long long>(m::{n}),' for n in sf.error_constants]
    out += ['};', '', 'using any_function = void (*)();', 'const any_function functions[] = {']
    out += [f'    reinterpret_cast<any_function>(&::{n}),' for n in sf.functions]
    out += ['};', '', 'const any_function mapped[] = {']
    out += [f'    reinterpret_cast<any_function>(&m::{n}),' for n in functions
            if n not in ('FT_LOAD_TARGET_', 'FT_LOAD_TARGET_MODE', 'FT_MAKE_TAG', 'FT_BOOL', 'FT_ERROR_BASE',
                         'FT_ERROR_MODULE', 'FT_CURVE_TAG')]
    out += ['};', '',
            '// The generic ones (abbreviated templates), each instantiated.',
            'static_assert(m::FT_LOAD_TARGET_(1) == 1 << 16 && m::FT_LOAD_TARGET_MODE(1 << 16) == FT_RENDER_MODE_LIGHT);',
            "static_assert(m::FT_MAKE_TAG('a', 'b', 'c', 'd') == 0x61626364u && m::FT_BOOL(2) == 1);",
            'static_assert(m::FT_ERROR_BASE(0x101) == 1 && m::FT_ERROR_MODULE(0x101) == 0x100u && m::FT_CURVE_TAG(5) == 1);',
            '',
            'int main() {',
            '    std::size_t missing = 0;',
            '    for (any_function f : functions)',
            '        missing += f == nullptr;',
            '    for (any_function f : mapped)',
            '        missing += f == nullptr;',
            f'    std::println("{len(sf.types) + len(tags)} types and tags, {len(sf.enumerators)} enumerators, '
            f'{len(constants) + len(sf.error_constants)} constants, {len(types)} aliases, {len(sf.functions)} functions '
            f'and {len(functions)} mapped functions reachable; {{}} constants summed", std::size(constants));',
            '    return missing == 0 ? 0 : 1;',
            '}']
    return '\n'.join(out) + '\n'


def outputs(root):
    sf = surface(fetch(root))
    kinds = classify(sf)
    unclassified = sorted(n for n, (k, _) in kinds.items() if k == 'unclassified')
    if unclassified:
        sys.exit('macros the classification does not know: ' + ', '.join(unclassified))
    return sf, kinds, {
        root / 'src/freetype.cppm': render_root(sf),
        root / 'src/macros_constants.cppm': render_constants(sf, kinds),
        root / 'include/mwrap/freetype/macros.hpp': render_side_header(sf, kinds),
        root / 'tests/exports_test.cpp': render_exports_test(sf, kinds),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    _, _, files = outputs(root)
    stale = []
    for path, text in files.items():
        current = path.read_bytes().decode('utf-8') if path.exists() else None
        if current == text:
            continue
        if args.check:
            stale.append(path.relative_to(root).as_posix())
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(text.encode('utf-8'))
            print('wrote', path.relative_to(root).as_posix())
    if stale:
        sys.exit('not what tools/gen_exports.py writes: ' + ', '.join(stale))


if __name__ == '__main__':
    main()
