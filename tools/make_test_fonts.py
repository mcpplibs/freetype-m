#!/usr/bin/env python3
"""Write tests/data/mwrap-test.ttf and tests/data/mwrap-test.otf, and the .ttf
compressed as gzip (.gz) and as Unix compress(1) LZW (.Z).

Two small fonts made for the tests, with what JetBrains Mono (the real-world
face the tests also load) does not have: a legacy 'kern' table, COLR v0
layers and a COLR v1 paint graph over a two-palette CPAL, an SVG glyph, a
format-14 cmap (Unicode variation sequences), vertical metrics, an fsType
and, in the .otf, CFF outlines with PostScript font info. Their outlines are
simple geometric shapes drawn here. The fonts are part of this repository
(Apache-2.0); the output is byte-stable (fixed timestamps), and
tests/data/README.md records the checksums. The compressed copies are what
the tests open through FT_Stream_OpenGzip / FT_Stream_OpenLZW; `gzip -dc`
decompresses both, which tools/make_test_fonts.py checks when gzip is on PATH.

Requires fontTools (tested with 4.67.0): python3 -m pip install fonttools
"""
import gzip
from pathlib import Path
import shutil
import subprocess

from fontTools.colorLib.builder import buildCOLR, buildCPAL
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.t2CharStringPen import T2CharStringPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import newTable
from fontTools.ttLib.tables._k_e_r_n import KernTable_format_0

UPEM = 1000
STAMP = 3_000_000_000  # 1999-01-24, seconds since 1904: fixed, so the output is byte-stable

# glyph name -> (advance, contours); each contour a list of on-curve points, or
# ('q', control, on) for a quadratic segment.
SHAPES = {
    '.notdef': (500, [[(50, 0), (450, 0), (450, 700), (50, 700)], [(100, 50), (100, 650), (400, 650), (400, 50)]]),
    'space': (250, []),
    'A': (600, [[(0, 0), (100, 0), (300, 600), (500, 0), (600, 0), (350, 700), (250, 700)]]),
    'V': (600, [[(250, 0), (350, 0), (600, 700), (500, 700), (300, 100), (100, 700), (0, 700)]]),
    'o': (600, [[(300, 0), ('q', (550, 0), (550, 250)), ('q', (550, 500), (300, 500)),
                 ('q', (50, 500), (50, 250)), ('q', (50, 0), (300, 0))]]),
    'heart': (800, [[(400, 0), (750, 400), ('q', (800, 700), (550, 700)), (400, 550), (250, 700),
                     ('q', (0, 700), (50, 400))]]),
    'heart.inner': (800, [[(400, 150), (600, 400), (550, 550), (400, 450), (250, 550), (200, 400)]]),
    'star': (800, [[(400, 750), (500, 450), (780, 450), (550, 280), (640, 0), (400, 170), (160, 0), (250, 280),
                    (20, 450), (300, 450)]]),
    'emptyset': (700, [[(100, 0), (600, 0), (600, 700), (100, 700)], [(150, 50), (150, 650), (550, 650), (550, 50)]]),
    'emptyset.slashed': (700, [[(100, 0), (600, 0), (600, 700), (100, 700)], [(150, 50), (150, 650), (550, 50)]]),
    'svgsquare': (800, [[(100, 0), (700, 0), (700, 600), (100, 600)]]),
}
ORDER = list(SHAPES)
CMAP = {0x20: 'space', 0x41: 'A', 0x56: 'V', 0x6F: 'o', 0x2665: 'heart', 0x2605: 'star', 0x2205: 'emptyset',
        0x25A0: 'svgsquare'}
UVS = [(0x2205, 0xFE00, 'emptyset.slashed')]
KERN = {('A', 'V'): -80, ('V', 'A'): -80, ('V', 'o'): -40}


def draw(pen, contours, quadratic):
    for contour in contours:
        start = contour[0]
        pen.moveTo(start)
        current = start
        for segment in contour[1:] + [start]:
            if isinstance(segment, tuple) and segment and segment[0] == 'q':
                _, control, on = segment
                if quadratic:
                    pen.qCurveTo(control, on)
                else:  # the quadratic as its exact cubic
                    c1 = (current[0] + 2 * (control[0] - current[0]) / 3, current[1] + 2 * (control[1] - current[1]) / 3)
                    c2 = (on[0] + 2 * (control[0] - on[0]) / 3, on[1] + 2 * (control[1] - on[1]) / 3)
                    pen.curveTo(c1, c2, on)
                current = on
            elif segment != start or current != start:
                pen.lineTo(segment)
                current = segment
        pen.closePath()


def common(fb, family, quadratic):
    # Side bearings from the outlines: lsb = xMin, tsb = vertical origin - yMax.
    bounds = {}
    for name in ORDER:
        pen = BoundsPen(None)
        draw(pen, SHAPES[name][1], quadratic)
        bounds[name] = pen.bounds or (0, 0, 0, 0)
    fb.setupCharacterMap(CMAP, uvs=UVS)
    fb.setupHorizontalMetrics({n: (SHAPES[n][0], round(bounds[n][0])) for n in ORDER})
    fb.setupHorizontalHeader(ascent=800, descent=-200)
    fb.setupNameTable({'familyName': family, 'styleName': 'Regular', 'uniqueFontIdentifier': family + ' test 1.0',
                       'fullName': family + ' Regular', 'psName': family.replace(' ', '') + '-Regular',
                       'version': 'Version 1.000', 'copyright': 'mcpp-community, Apache-2.0'})
    # fsType 0x0008: editable embedding (FT_FSTYPE_EDITABLE_EMBEDDING).
    fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200, fsType=0x0008,
                usWeightClass=400, achVendID='MCPP')
    fb.setupPost()
    fb.setupVerticalHeader(ascent=500, descent=-500)
    fb.setupVerticalMetrics({n: (1000, 900 - round(bounds[n][3])) for n in ORDER})
    fb.font['head'].created = fb.font['head'].modified = STAMP
    kern = newTable('kern')
    kern.version = 0
    sub = KernTable_format_0()
    sub.version, sub.coverage, sub.format, sub.tupleIndex = 0, 1, 0, 0
    sub.kernTable = dict(KERN)
    kern.kernTables = [sub]
    fb.font['kern'] = kern
    palettes = [[(0.8, 0.1, 0.2, 1.0), (1.0, 0.8, 0.9, 1.0)], [(0.1, 0.2, 0.8, 1.0), (0.8, 0.9, 1.0, 0.5)]]
    fb.font['CPAL'] = buildCPAL(palettes)
    fb.font['COLR'] = buildCOLR(
        {
            # COLR v0: two layers, palette entries 0 and 1.
            'heart': [('heart', 0), ('heart.inner', 1)],
            # COLR v1: two layers, the star filled with a linear gradient over
            # both entries, and the heart's inner shape in a solid colour.
            'star': {
                'Format': 1,  # PaintColrLayers
                'Layers': [
                    {
                        'Format': 10,  # PaintGlyph
                        'Glyph': 'star',
                        'Paint': {
                            'Format': 4,  # PaintLinearGradient
                            'ColorLine': {'Extend': 'pad', 'ColorStop': [
                                {'StopOffset': 0.0, 'PaletteIndex': 0, 'Alpha': 1.0},
                                {'StopOffset': 1.0, 'PaletteIndex': 1, 'Alpha': 1.0}]},
                            'x0': 0, 'y0': 0, 'x1': 800, 'y1': 0, 'x2': 0, 'y2': 800,
                        },
                    },
                    {
                        'Format': 10,  # PaintGlyph
                        'Glyph': 'heart.inner',
                        'Paint': {'Format': 2, 'PaletteIndex': 1, 'Alpha': 0.5},  # PaintSolid
                    },
                ],
            },
        },
        clipBoxes={'star': (0, 0, 800, 750)},
    )
    svg = newTable('SVG ')
    gid = ORDER.index('svgsquare')
    svg.docList = [(f'<svg xmlns="http://www.w3.org/2000/svg"><rect id="glyph{gid}" x="100" y="-600" width="600" '
                    'height="600" fill="#2050c0"/></svg>', gid, gid)]
    fb.font['SVG '] = svg
    fb.font.recalcTimestamp = False


def ttf(path):
    fb = FontBuilder(UPEM, isTTF=True)
    fb.setupGlyphOrder(ORDER)
    glyphs = {}
    for name in ORDER:
        pen = TTGlyphPen(None)
        draw(pen, SHAPES[name][1], quadratic=True)
        glyphs[name] = pen.glyph()
    fb.setupGlyf(glyphs)
    common(fb, 'mwrap test', quadratic=True)
    fb.save(path)


def otf(path):
    fb = FontBuilder(UPEM, isTTF=False)
    fb.setupGlyphOrder(ORDER)
    charstrings = {}
    for name in ORDER:
        pen = T2CharStringPen(SHAPES[name][0], None)
        draw(pen, SHAPES[name][1], quadratic=False)
        charstrings[name] = pen.getCharString()
    fb.setupCFF('mwraptestCFF-Regular', {'FullName': 'mwrap test CFF Regular', 'FamilyName': 'mwrap test CFF',
                                         'Weight': 'Regular', 'Notice': 'mcpp-community, Apache-2.0'},
                charstrings, {'BlueValues': [-10, 0, 700, 710], 'StdHW': 100, 'StdVW': 100})
    common(fb, 'mwrap test CFF', quadratic=False)
    fb.save(path)


def lzw_compress(data: bytes, max_bits: int = 16) -> bytes:
    """compress(1) LZW, block mode: what `compress -b16` writes (no CLEAR codes).

    Codes start at 9 bits and are packed LSB first in groups of eight; when the
    code width grows, the current group is padded to its full size, as
    compress's output() does and every decoder (ftzopen.c, gzip's unlzw)
    expects.
    """
    out = bytearray([0x1F, 0x9D, 0x80 | max_bits])
    n_bits, free_ent, max_max = 9, 257, 1 << max_bits
    max_code = (1 << n_bits) - 1
    group, bits, table = 0, 0, {}

    def flush_group(width):
        nonlocal group, bits
        out.extend(group.to_bytes(width, 'little'))
        group, bits = 0, 0

    def output(code):
        nonlocal n_bits, max_code, group, bits
        group |= code << bits
        bits += n_bits
        if bits == n_bits * 8:
            flush_group(n_bits)
        if free_ent > max_code:
            if bits:
                flush_group(n_bits)
            n_bits += 1
            max_code = max_max if n_bits == max_bits else (1 << n_bits) - 1

    ent = data[0]
    for c in data[1:]:
        key = (ent, c)
        if key in table:
            ent = table[key]
            continue
        output(ent)
        ent = c
        if free_ent < max_max:
            table[key] = free_ent
            free_ent += 1
    output(ent)
    if bits:
        out.extend(group.to_bytes((bits + 7) // 8, 'little'))
    return bytes(out)


if __name__ == '__main__':
    data = Path(__file__).resolve().parents[1] / 'tests/data'
    data.mkdir(parents=True, exist_ok=True)
    ttf(data / 'mwrap-test.ttf')
    otf(data / 'mwrap-test.otf')
    font = (data / 'mwrap-test.ttf').read_bytes()
    (data / 'mwrap-test.ttf.gz').write_bytes(gzip.compress(font, compresslevel=9, mtime=0))
    (data / 'mwrap-test.ttf.Z').write_bytes(lzw_compress(font))
    if shutil.which('gzip'):
        for name in ('mwrap-test.ttf.gz', 'mwrap-test.ttf.Z'):
            back = subprocess.run(['gzip', '-dc', str(data / name)], capture_output=True, check=True).stdout
            assert back == font, name + ' does not decompress to mwrap-test.ttf'
    for name in ('mwrap-test.ttf', 'mwrap-test.otf', 'mwrap-test.ttf.gz', 'mwrap-test.ttf.Z'):
        print('wrote', (data / name).relative_to(data.parents[1]).as_posix())
