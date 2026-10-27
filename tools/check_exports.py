#!/usr/bin/env python3
"""Audit the module surface against the pinned release's public headers.

- src/freetype.cppm, src/macros_constants.cppm, the side header and
  tests/exports_test.cpp are what tools/gen_exports.py writes (fresh).
- Every function (FT_EXPORT), type, struct / union / enum tag and enumerator
  of a named enum of the public headers is exported by src/freetype.cppm.
- Every macro of the public and configuration headers is accounted for:
  mapped to a constant or an alias (src/macros_constants.cppm), mapped to a
  function (src/macros_functions.cppm, which sets the macro aside first),
  kept as a macro in <mwrap/freetype/macros.hpp> with the reason, or excluded
  with the reason. Every FT_Err_* / FT_Mod_Err_* code is a constant.
- The headers left out (tools/inventory.py NOT_HEADERS) exist upstream.

tests/exports_test.cpp then names all of it through the import alone, and
takes every function's address, on every CI row.
"""
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_exports import FUNCTIONS, outputs  # noqa: E402
from inventory import NOT_HEADERS, fetch  # noqa: E402


def main():
    root = Path(__file__).resolve().parents[1]
    problems = []
    generated = subprocess.run([sys.executable, str(root / 'tools/gen_exports.py'), '--check'],
                               capture_output=True, text=True)
    if generated.returncode != 0:
        problems.append(generated.stderr.strip() or generated.stdout.strip())
    sf, kinds, _ = outputs(root)

    exported = set(re.findall(r'^export using ::(\w+);', (root / 'src/freetype.cppm').read_text(encoding='utf-8'),
                              flags=re.M))
    for what, names in (('functions', sf.functions), ('types', sf.types), ('tags', sf.tags),
                        ('enumerators', sf.enumerators)):
        missing = set(names) - exported
        if missing:
            problems.append(f'src/freetype.cppm does not export these {what}: ' + ', '.join(sorted(missing)))

    constants = (root / 'src/macros_constants.cppm').read_text(encoding='utf-8')
    purview = constants[constants.index('export module'):]
    mapped = set(re.findall(r'^inline constexpr (?:auto|FT_Error) (\w+) =', purview, flags=re.M))
    aliases = set(re.findall(r'^using (\w+) =', purview, flags=re.M))
    for kind, have, label in (('constant', mapped, 'constants'), ('type', aliases, 'aliases')):
        missing = {n for n, (k, _) in kinds.items() if k == kind} - have
        if missing:
            problems.append(f'macros not mapped to {label}: ' + ', '.join(sorted(missing)))
    missing = set(sf.error_constants) - mapped
    if missing:
        problems.append('error codes not mapped: ' + ', '.join(sorted(missing)))

    functions = (root / 'src/macros_functions.cppm').read_text(encoding='utf-8')
    head, body = functions.split('export module', 1)
    undefined = set(re.findall(r'^#undef (\w+)', head, flags=re.M))
    defined = set(re.findall(r'^(?:constexpr|inline)\s[^(]*?\b(\w+)\(', body, flags=re.M))
    wanted = {n for n, (k, _) in kinds.items() if k == 'function'}
    if wanted != set(FUNCTIONS):
        problems.append('FUNCTIONS lists names that are not function-like macros of the headers: '
                        + ', '.join(sorted(set(FUNCTIONS) - wanted)))
    if wanted - defined:
        problems.append('mapped functions not defined in src/macros_functions.cppm: ' + ', '.join(sorted(wanted - defined)))
    if defined - wanted:
        problems.append('functions in src/macros_functions.cppm that map no macro: ' + ', '.join(sorted(defined - wanted)))
    if wanted - undefined:
        problems.append('macros not set aside (#undef) before their functions: ' + ', '.join(sorted(wanted - undefined)))

    unaccounted = {n for n, (k, _) in kinds.items() if k == 'unclassified'}
    if unaccounted:
        problems.append('macros neither mapped, kept nor excluded: ' + ', '.join(sorted(unaccounted)))
    include = fetch(root) / 'include/freetype'
    stale = [h for h in NOT_HEADERS if not (include / h).exists()]
    if stale:
        problems.append('left-out headers that do not exist: ' + ', '.join(stale))
    if problems:
        sys.exit('\n'.join(problems))

    count = {k: sum(1 for kk, _ in kinds.values() if kk == k) for k in ('constant', 'type', 'function', 'side', 'excluded')}
    print(f'{len(sf.headers)} public headers ({len(NOT_HEADERS)} left out, with reasons): {len(sf.functions)} functions, '
          f'{len(sf.types)} types, {len(sf.tags)} tags and {len(sf.enumerators)} enumerators exported; '
          f'{len(kinds)} macros: {count["constant"]} constants, {count["type"]} aliases, {count["function"]} functions, '
          f'{count["side"]} kept as macros, {count["excluded"]} excluded; {len(sf.error_constants)} error codes mapped')


if __name__ == '__main__':
    main()
