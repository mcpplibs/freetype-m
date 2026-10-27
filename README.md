# freetype

> FreeType 2, the portable font engine, through C++23 modules.

**English** · [简体中文](README.zh.md) · [Upstream](https://gitlab.freedesktop.org/freetype/freetype) · [Releases](https://github.com/mcpplibs/freetype-m/releases) · [mcpp](https://github.com/mcpp-community/mcpp) · [Index](https://github.com/mcpplibs/mcpp-index)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![FreeType](https://img.shields.io/badge/FreeType-FTL-blue.svg)](upstream/freetype/docs/FTL.TXT)
[![mcpp-community](https://img.shields.io/badge/mcpp--community-mwrap-blue)](https://github.com/mcpp-community)
[![CI](https://github.com/mcpplibs/freetype-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/mcpplibs/freetype-m/actions/workflows/ci.yml)

The upstream C API is unchanged. `FT_Library`, `FT_New_Memory_Face`,
`FT_Load_Glyph`, `FT_Outline_Decompose` and `FT_Stroker_New` are the
upstream globals of FreeType's public headers: every header `<ft2build.h>`
names, from `freetype.h` to the cache, stroker, multiple-master, colour and
SFNT-table APIs. The library is `compat.freetype` 2.13.3. The macros are
constants and constexpr functions. An audit checks every declaration and
macro against the release's headers, and FreeType's own test programs pass
on every CI run, ported onto the module.

## Modules

- `import mcpplibs.mwrap.freetype;`: the whole API (218 functions, every type and enumerator); re-exports the one below
- `import mcpplibs.mwrap.freetype.macros;`: `FT_LOAD_RENDER`, `FT_FACE_FLAG_KERNING`, `FT_Err_Ok`, `FT_HAS_KERNING`, `FT_MAKE_TAG` … in `mcpplibs::mwrap::freetype::macros`

> More Modules API -> [Usage and API](docs/usage.md)

## Getting started

```sh
mcpp new banner --template mcpplibs.mwrap.freetype@2.13.3
cd banner
mcpp run                                   # a star and a stroked ring, rasterised
mcpp run -- /path/to/font.ttf "Hello"      # text in any font, kerned
```

The default template builds an outline point by point and strokes a ring
around it. FreeType rasterises both and the program prints them on the
terminal. Given a font, it lays out text with the font's advances and
kerning instead.

```toml
[dependencies]
"mcpplibs.mwrap.freetype" = "2.13.3"
```

```cpp
import std;
import mcpplibs.mwrap.freetype;
using namespace mcpplibs::mwrap::freetype::macros;  // FT_LOAD_RENDER, FT_Err_Ok …

int main(int argc, char** argv) {
    FT_Library lib{};
    FT_Init_FreeType(&lib);
    FT_Face face{};
    if (argc < 2 || FT_New_Face(lib, argv[1], 0, &face) != FT_Err_Ok) return 1;
    FT_Set_Pixel_Sizes(face, 0, 32);
    FT_Load_Char(face, 'A', FT_LOAD_RENDER);
    const FT_Bitmap& b = face->glyph->bitmap;
    std::println("{} {}: 'A' is {}x{} px, kerning {}", face->family_name, face->style_name,
                 b.width, b.rows, FT_HAS_KERNING(face) ? "yes" : "no");
    FT_Done_Face(face);
    FT_Done_FreeType(lib);
}
```

## Macros

Named modules cannot export macros. FreeType's 1274 macros are mapped as
follows:

- 1013 constants, each of its macro's type.
- The `FT_Err_*` codes, which are unnamed enumerators in the headers, as
  `FT_Error` constants.
- 22 type aliases.
- 28 functions: `FT_HAS_*` / `FT_IS_*`, `FT_LOAD_TARGET_`,
  `FT_LOAD_TARGET_MODE`, `FT_MAKE_TAG`, `FT_CURVE_TAG` …

Macros that a constant or function cannot be stay in
`<mwrap/freetype/macros.hpp>`, included before the import: include names
(`FT_FREETYPE_H` …), `FT_ENC_TAG`, the token-pasting `FT_ERR_EQ`, the
error-table X-macros and the configuration switches. The library's
configuration, the left-out Carbon header `ftmac.h` and mixing the headers
with the import are described in [docs/usage.md](docs/usage.md).

## Examples

- [examples/ftdump](examples/ftdump) describes font files: faces, format
  and driver, flags, metrics, character maps, variation axes, names, tables,
  palettes, PostScript and BDF properties.
- [conformance](conformance): FreeType's `tests/issue-1063`,
  `src/tools/test_trig.c` and `src/tools/test_bbox.c`, ported onto the
  module. [tools/run_conformance.py](tools/run_conformance.py) runs them
  with upstream's test font, checks the bounding boxes against exact ones,
  and runs upstream's `apinames` over the headers against the exports.

## Build and testing

Linux (GCC and Clang), macOS and Windows CI build the modules and run:

- the tests: the API on real and purpose-made fonts (rendering in every
  mode, outlines, stroker, bitmaps, colour glyphs, SVG hooks, cache,
  variations, kerning, compressed streams), the macros, the headers mixed
  with the import, and the whole surface named through the import;
- the export audit ([tools/check_exports.py](tools/check_exports.py));
- FreeType's own tests;
- ftdump.

A separate job scaffolds the default template as an installed package and
runs it.

## License

The wrapper is [Apache-2.0](LICENSE), by mcpp-community ([NOTICE](NOTICE)).
FreeType retains its own licenses: it is used under the FreeType License
([upstream/freetype/docs/FTL.TXT](upstream/freetype/docs/FTL.TXT), chosen
per [LICENSE.TXT](upstream/freetype/LICENSE.TXT)), with the BDF and PCF
driver notices in [upstream/freetype/src](upstream/freetype/src). Portions
of this software are copyright © 2024 The FreeType Project
(www.freetype.org). All rights reserved.
