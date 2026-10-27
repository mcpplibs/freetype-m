#!/usr/bin/env python3
"""Run FreeType's own test programs, ported onto import mcpplibs.mwrap.freetype.

conformance/ holds the test programs of the pinned release, each a port of
upstream's source onto the module (C++ for C; the logic and output are
upstream's). This script builds them, gets their data and checks them:

- tests/issue-1063/main.c, the regression test upstream's `meson test`
  runs: it loads glyphs 59-170 of "As I Lay Dying". The font comes from the
  list in upstream's tests/scripts/download-test-fonts.py (URL and digest
  read from that script, the download checked against the digest). It
  passes as upstream's does: exit status 0; here no glyph may be reported
  unknown either.
- src/tools/test_trig.c: FT_Cos, FT_Sin, FT_Tan, FT_Vector_Unit,
  FT_Vector_Length and FT_Vector_Rotate against the C library over the
  whole circle, within upstream's threshold: its "ok" line and no failure
  line. (Its FT_Atan2 lines are reports, not failures, upstream as here;
  and it returns `!error`, 1 when it passes, which is kept.)
- src/tools/test_bbox.c: the control and exact boxes of its three cubic
  outlines (FT_Outline_Get_CBox, FT_Outline_Get_BBox), compared with boxes
  computed here independently (each cubic's extremes, solved exactly)
  within 2/65536; outline 3's is [0 100 128 128], as its comment says.

And src/tools/apinames.c, the program upstream's build uses to list the
exported API (the .def / linker map of the shared library), is compiled
unchanged from the release (a C program, with its vms_shorten_symbol.c)
and run over include/freetype/*.h and config/*.h: every name it lists is
exported by src/freetype.cppm, except those of ftmac.h (left out, with the
reason, in tools/inventory.py).

src/tools/test_afm.c is not ported: it tests the AFM parser through
FreeType's internal headers (freetype/internal/psaux.h), which are not
public API and not part of the module.
"""
import argparse
import ast
import hashlib
import io
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import urllib.request
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from inventory import NOT_HEADERS, fetch  # noqa: E402


def test_fonts(upstream: Path, root: Path) -> Path:
    """The fonts of upstream's download-test-fonts.py, into target/upstream/test-fonts."""
    script = (upstream / 'tests/scripts/download-test-fonts.py').read_text(encoding='utf-8')
    items = next(ast.literal_eval(node.value) for node in ast.parse(script).body
                 if isinstance(node, ast.Assign) and any(getattr(t, 'id', '') == '_DOWNLOAD_ITEMS' for t in node.targets))
    out = root / 'target/upstream/test-fonts'
    out.mkdir(parents=True, exist_ok=True)

    def digest(data: bytes) -> str:  # upstream's digest_data: the first 8 bytes of SHA-256
        return hashlib.sha256(data).digest()[:8].hex()

    for item in items:
        files = item.get('zip_files') or [item]
        wanted = [out / (f.get('install_name') or Path(f.get('filename') or item['file_url']).name) for f in files]
        if all(w.exists() and digest(w.read_bytes()) == f.get('hex_digest', digest(w.read_bytes()))
               for w, f in zip(wanted, files)):
            continue
        url = item.get('zip_url') or item['file_url']
        print('+ fetch', url, flush=True)
        with urllib.request.urlopen(url, timeout=180) as response:
            content = response.read()
        for target, f in zip(wanted, files):
            data = zipfile.ZipFile(io.BytesIO(content)).read(f['filename']) if 'zip_url' in item else content
            if 'hex_digest' in f and digest(data) != f['hex_digest']:
                sys.exit(f'{target.name}: digest {digest(data)} != {f["hex_digest"]} (download-test-fonts.py)')
            target.write_bytes(data)
    return out


def build(project: Path, mcpp: str, toolchain: str | None) -> Path:
    command = [mcpp, 'build', '--profile', 'release'] + (['--toolchain', toolchain] if toolchain else [])
    print('+', ' '.join(command), flush=True)
    subprocess.run(command, cwd=project, check=True)
    exe = '.exe' if os.name == 'nt' else ''
    found = sorted((p for p in (project / 'target').rglob('test_trig' + exe) if p.parent.name == 'bin'),
                   key=lambda p: p.stat().st_mtime)
    if not found:
        sys.exit('conformance/ was not built')
    return found[-1].parent


def apinames(upstream: Path, root: Path, mcpp: str, toolchain: str | None) -> list[str]:
    """Upstream's apinames over the public headers, against the module's exports."""
    project = root / 'target/conformance-apinames'
    (project / 'src').mkdir(parents=True, exist_ok=True)
    (project / 'inc').mkdir(parents=True, exist_ok=True)
    tools = upstream / 'src/tools'
    files = {project / 'src/main.c': (tools / 'apinames.c').read_bytes(),
             project / 'inc/vms_shorten_symbol.c': (tools / 'vms_shorten_symbol.c').read_bytes(),
             project / 'mcpp.toml': b'[package]\nname = "apinames"\nversion = "0.5.0"\n\n[targets.apinames]\n'
                                    b'kind = "bin"\nmain = "src/main.c"\n\n[build]\nsources = ["src/main.c"]\n'
                                    b'include_dirs = ["inc"]\n'}
    for path, data in files.items():
        if not path.exists() or path.read_bytes() != data:
            path.write_bytes(data)
    command = [mcpp, 'build'] + (['--toolchain', toolchain] if toolchain else [])
    print('+', ' '.join(command), '(src/tools/apinames.c)', flush=True)
    subprocess.run(command, cwd=project, check=True)
    exe = 'apinames.exe' if os.name == 'nt' else 'apinames'
    binary = max((p for p in (project / 'target').rglob(exe) if p.parent.name == 'bin'), key=lambda p: p.stat().st_mtime)
    include = upstream / 'include/freetype'

    def names(headers):
        r = subprocess.run([str(binary), *map(str, headers)], capture_output=True, text=True, check=True)
        return {line.strip() for line in r.stdout.splitlines() if line.strip()}

    listed = names(sorted(include.glob('*.h')) + sorted((include / 'config').glob('*.h')))
    left_out = names([include / 'ftmac.h'])  # NOT_HEADERS: the Carbon API
    exported = set(re.findall(r'^export using ::(\w+);', (root / 'src/freetype.cppm').read_text(encoding='utf-8'),
                              flags=re.M))
    missing = listed - left_out - exported
    if missing:
        return ['apinames lists names the module does not export: ' + ', '.join(sorted(missing))]
    print(f'  apinames: {len(listed)} API names; {len(listed - left_out)} exported, '
          f'{len(left_out)} of ftmac.h left out ({", ".join(sorted(left_out))})')
    return []


def run(binary: Path, env=None) -> subprocess.CompletedProcess:
    exe = binary.with_suffix('.exe') if os.name == 'nt' else binary
    print('+', exe.name, flush=True)
    return subprocess.run([str(exe)], capture_output=True, text=True, env=env, timeout=600)


def cubic_box(p0, p1, p2, p3):
    """The exact box of a cubic Bézier: its ends and the extremes inside."""
    xs, ys = [p0[0], p3[0]], [p0[1], p3[1]]
    for axis, out in ((0, xs), (1, ys)):
        a0, a1, a2, a3 = p0[axis], p1[axis], p2[axis], p3[axis]
        # B'(t)/3 = a t^2 + b t + c
        a = -a0 + 3 * a1 - 3 * a2 + a3
        b = 2 * (a0 - 2 * a1 + a2)
        c = a1 - a0
        roots = []
        if abs(a) < 1e-12:
            if abs(b) > 1e-12:
                roots.append(-c / b)
        else:
            disc = b * b - 4 * a * c
            if disc >= 0:
                roots += [(-b + s * math.sqrt(disc)) / (2 * a) for s in (1, -1)]
        for t in roots:
            if 0 < t < 1:
                mt = 1 - t
                out.append(mt ** 3 * a0 + 3 * mt * mt * t * a1 + 3 * mt * t * t * a2 + t ** 3 * a3)
    return min(xs), min(ys), max(xs), max(ys)


def check_bbox(output: str) -> list[str]:
    # The outlines of test_bbox.c: XVEC(x, y) is (FT_Pos)(x * 65536), truncated.
    xx = lambda v: math.trunc(v * 65536)  # noqa: E731
    outlines = [
        [(408.9111, 535.3164), (455.8887, 634.396), (-37.8765, 786.2207), (164.6074, 535.3164)],
        [(100.0, 100.0), (100.0, 200.0), (200.0, 200.0), (200.0, 133.0)],
        [(100.0, 127.0), (200.0, 127.0), (0.0, 136.0), (0.0, 100.0)],
    ]
    hexes = re.findall(r'(cbox|bbox)_hex = \[(\w+) (\w+) (\w+) (\w+)\]', output)
    if len(hexes) != 6:
        return [f'test_bbox printed {len(hexes)} boxes, not 6']
    problems = []
    for n, points in enumerate(outlines):
        pts = [(xx(x), xx(y)) for x, y in points]
        expected = {
            'cbox': (min(p[0] for p in pts), min(p[1] for p in pts), max(p[0] for p in pts), max(p[1] for p in pts)),
            'bbox': cubic_box(*pts),
        }
        for kind, *values in hexes[2 * n: 2 * n + 2]:
            got = [int(v, 16) - (1 << 32) if int(v, 16) >= 1 << 31 else int(v, 16) for v in values]
            want = expected[kind]
            if any(abs(g - w) > 2 for g, w in zip(got, want)):
                problems.append(f'outline #{n + 1} {kind} {got} != {[round(w, 1) for w in want]}')
            else:
                print(f'  outline #{n + 1} {kind} [{" ".join(f"{g / 65536:.4f}" for g in got)}] matches')
    if '  0.0000 100.0000 128.0000 128.0000]' not in output.split('outline #3')[1].split('bbox_hex')[0]:
        problems.append('outline #3 bbox is not [0 100 128 128]')
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--mcpp', default='mcpp')
    parser.add_argument('--toolchain')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    upstream = fetch(root)
    fonts = test_fonts(upstream, root)
    bin_dir = build(root / 'conformance', args.mcpp, args.toolchain)
    problems = []

    r = run(bin_dir / 'issue_1063', env=dict(os.environ, FREETYPE_TESTS_DATA_DIR=str(fonts)))
    print(r.stdout + r.stderr, end='')
    if r.returncode != 0 or 'unknown' in r.stdout:
        problems.append(f'issue-1063: exit {r.returncode}')
    else:
        print('  issue-1063: glyphs 59-170 of As.I.Lay.Dying.ttf load')

    r = run(bin_dir / 'test_trig')
    reports = len(re.findall(r'^FT_Atan2', r.stdout, flags=re.M))
    failures = [line for line in r.stdout.splitlines() if not line.startswith('FT_Atan2') and 'ok !' not in line]
    if r.returncode != 1 or 'trigonometry test ok !' not in r.stdout or failures:
        problems.append(f'test_trig: exit {r.returncode}\n' + '\n'.join(failures[:20]))
    else:
        print(f'  test_trig: trigonometry test ok ! ({reports} FT_Atan2 reports, not failures upstream)')

    r = run(bin_dir / 'test_bbox')
    print(r.stdout, end='')
    problems += ['test_bbox: ' + p for p in check_bbox(r.stdout)] if r.returncode == 0 else [f'test_bbox: exit {r.returncode}']

    problems += apinames(upstream, root, args.mcpp, args.toolchain)

    if problems:
        sys.exit('\n'.join(problems))
    print('FreeType conformance: issue-1063, test_trig and test_bbox pass on the module; '
          'apinames finds every API name exported')


if __name__ == '__main__':
    main()
