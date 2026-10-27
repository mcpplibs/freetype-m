# Using mcpplibs.mwrap.freetype

`import mcpplibs.mwrap.freetype;` declares FreeType 2.13.3's public API as its headers do: `<freetype/freetype.h>` and every header `<ft2build.h>` names (`FT_GLYPH_H`, `FT_OUTLINE_H`, `FT_STROKER_H`, `FT_CACHE_H`, `FT_MULTIPLE_MASTERS_H`, `FT_COLOR_H`, `FT_TRUETYPE_TABLES_H` …), plus `ftlogging.h`. Functions, types and enumerators are the upstream globals. They are the very same entities as the headers', so a translation unit may include the headers too. The library is `compat.freetype`, built from the same release.

## Modules

| Module | What it holds |
|---|---|
| `mcpplibs.mwrap.freetype` | every function (`FT_Init_FreeType`, `FT_New_Memory_Face`, `FT_Load_Glyph`, `FT_Outline_Decompose`, `FT_Stroker_New`, `FTC_Manager_New` … 218), type and struct tag (`FT_Face`, `FT_FaceRec`, `FT_Outline`, `FT_Bitmap`, `FT_Glyph`, `TT_OS2`, `FT_COLR_Paint` … 357), and enumerator of a named enum (`FT_ENCODING_UNICODE`, `FT_RENDER_MODE_LCD`, `FT_PIXEL_MODE_BGRA`, `FT_COLR_PAINTFORMAT_GLYPH` … 209); re-exports the one below |
| `mcpplibs.mwrap.freetype.macros` | the macros, in `mcpplibs::mwrap::freetype::macros` |

```cpp
import mcpplibs.mwrap.freetype;
using namespace mcpplibs::mwrap::freetype::macros;  // FT_LOAD_RENDER, FT_HAS_KERNING, FT_Err_Ok …
```

[tools/check_exports.py](../tools/check_exports.py) audits this surface against the pinned headers. [tests/exports_test.cpp](../tests/exports_test.cpp) names every exported entity through the import alone and takes every function's address, so each must also link. Three headers are left out:

| Header | Why |
|---|---|
| `ftmac.h` | the Classic Mac OS / Carbon API. Its declarations use Carbon types (`Handle`, `FSSpec`, `FSRef`), and `src/base/ftmac.c` builds them only under `FT_MACINTOSH`, which no supported target defines |
| `ftchapters.h` | documentation chapters, with no declarations |
| `fterrdef.h` | the error list that `fterrors.h` reads as an X-macro; its entries are the `FT_Err_*` constants |

## Macros

A named module cannot export a macro. `mcpplibs::mwrap::freetype::macros` holds them under their original names:

- **Constants**, 1013 of them, as `inline constexpr`, each with its macro's type (`1L << n` is a `long`). They include `FT_LOAD_*` (with `FT_LOAD_TARGET_NORMAL` … `FT_LOAD_TARGET_LCD_V`), `FT_FACE_FLAG_*`, `FT_STYLE_FLAG_*`, `FT_OPEN_*`, `FT_OUTLINE_*`, `FT_CURVE_TAG_*`, `FT_RASTER_FLAG_*`, `FT_SUBGLYPH_FLAG_*`, `FT_FSTYPE_*`, `FT_GASP_*`, `FT_PARAM_TAG_*`, `FT_VALIDATE_*`, `FT_ANGLE_*`, `TTAG_*`, the `ttnameid.h` identifiers (`TT_PLATFORM_*`, `TT_MS_ID_*`, `TT_MS_LANGID_*`, `TT_NAME_ID_*`, `TT_UCR_*` …), `T1_*`, `FREETYPE_MAJOR` / `MINOR` / `PATCH`, the configuration's numbers, and the legacy spellings (`ft_encoding_unicode`, `ft_render_mode_mono`, `ft_kerning_default`, `ft_glyph_bbox_pixels` …).
- **Error codes**: `FT_Err_*` and `FT_Mod_Err_*` are enumerators of unnamed enums in the headers. A type with no name is TU-local and cannot be exported, so here they are `FT_Error` constants.
- **Types**: the macros that name a type are aliases, such as `FT_Outline_MoveTo_Func`, `FT_Raster_Span_Func`, `FTRenderer_render` and the platform's `FT_INT64` / `FT_UINT64`.
- **Functions** with the macros' results, constexpr where the macro is arithmetic:
  - Face flags: `FT_HAS_HORIZONTAL`, `FT_HAS_VERTICAL`, `FT_HAS_KERNING`, `FT_IS_SCALABLE`, `FT_IS_SFNT`, `FT_IS_FIXED_WIDTH`, `FT_HAS_FIXED_SIZES`, `FT_HAS_GLYPH_NAMES`, `FT_HAS_MULTIPLE_MASTERS`, `FT_IS_NAMED_INSTANCE`, `FT_IS_VARIATION`, `FT_IS_CID_KEYED`, `FT_IS_TRICKY`, `FT_HAS_COLOR`, `FT_HAS_SVG`, `FT_HAS_SBIX`, `FT_HAS_SBIX_OVERLAY`, `FT_HAS_FAST_GLYPHS`.
  - Arithmetic: `FT_LOAD_TARGET_`, `FT_LOAD_TARGET_MODE`, `FT_MAKE_TAG`, `FT_CURVE_TAG`, `FT_BOOL`, `FT_ERROR_BASE`, `FT_ERROR_MODULE`, `FT_IS_EMPTY`, `FTC_IMAGE_TYPE_COMPARE`.
  - `FT_FACE_DRIVER_NAME`, which is not constexpr because it reads through the driver class.

  Each argument is evaluated once, as in the macros. The generic macros (`FT_MAKE_TAG`, `FT_CURVE_TAG`, `FT_BOOL`, `FT_ERROR_BASE` …) are function templates over their argument types.

### What stays a macro

These stay macros in `<mwrap/freetype/macros.hpp>`, which is the public headers:

| Macro | Why |
|---|---|
| `FT_FREETYPE_H`, `FT_GLYPH_H` … (65) | an `#include` operand |
| `FT_ENC_TAG`, `FT_IMAGE_TAG` | expand to an enumerator definition (`value = …`) inside an enum |
| `FT_ERR`, `FT_ERR_EQ`, `FT_ERR_NEQ`, `FT_ERR_CAT`, `FT_ERR_XCAT`, `FT_VALIDATE_GX_BITFIELD` | paste tokens into names |
| `FT_ERRORDEF`, `FT_ERRORDEF_`, `FT_NOERRORDEF_`, `FT_ERROR_START_LIST`, `FT_ERROR_END_LIST`, `FT_ERR_PREFIX`, `FT_ERR_BASE`, `FT_MODERRDEF`, `FT_MODERR_START_LIST`, `FT_MODERR_END_LIST` | the X-macro machinery that generates error tables from `fterrors.h` / `ftmoderr.h` |
| `FT_UNUSED` | assigns any lvalue to itself |
| `FT_STATIC_CAST`, `FT_REINTERPRET_CAST`, `FT_STATIC_BYTE_CAST` | take a type as an argument |
| `FT_CONFIG_OPTION_*`, `TT_CONFIG_*`, `AF_CONFIG_*` … | configuration switches for `#ifdef`, defined empty |

Not part of a program's API: include guards, the declaration annotations (`FT_EXPORT`, `FT_PUBLIC_FUNCTION_ATTRIBUTE`, `FT_BEGIN_HEADER`, `FT_END_HEADER`), and `config/ftstdlib.h`'s mapping of the C library for FreeType's implementation (`ft_memcpy` is `memcpy`, `FT_FILE` is `FILE` …). Its limits (`FT_INT_MAX`, `FT_CHAR_BIT` …) are mapped. `tools/check_exports.py` accounts for all 1274 macros.

### Headers and the import together

Include the side header before the import, as [tests/header_mix_test.cpp](../tests/header_mix_test.cpp) does:

```cpp
#include <mwrap/freetype/macros.hpp>
#include FT_ERRORS_H
import mcpplibs.mwrap.freetype;
```

- While a macro is defined it expands even inside a qualified name. `macros::FT_LOAD_RENDER` is only reachable with the macro set aside (`#pragma push_macro` / `#undef`); otherwise use the plain macro.
- With the headers included, `FT_Err_*` are also the headers' enumerators. Do not add `using namespace …::macros;` in such a file, which would make them ambiguous, or qualify the module's `macros::FT_Err_Ok`.
- `fterrors.h` undefines `FT_ERR_PREFIX` outside the library's own build. A program that uses `FT_ERR` / `FT_ERR_EQ` / `FT_ERR_NEQ` defines it (`#define FT_ERR_PREFIX FT_Err_`), as it would in C.
- The error-message table is generated the documented way: `#undef FTERRORS_H_`, define `FT_ERRORDEF` / `FT_ERROR_START_LIST` / `FT_ERROR_END_LIST`, include `FT_ERRORS_H` again.

## The library's configuration

`compat.freetype` 2.13.3 builds upstream's default module set (`modules.cfg`, `CMakeLists.txt` `BASE_SRCS`) with upstream's default `ftoption.h`:

- Drivers: TrueType (bytecode interpreter v40, GX variations), CFF, Type 1, CID, Type 42, PFR, Windows FNT, PCF, BDF. Renderers: smooth, raster (mono), SDF, bitmap SDF, OT-SVG (with your hooks). Also the autofitter, the cache subsystem, and gzip and LZW streams with the bundled zlib (WOFF 1).
- Off, as upstream ships it: libpng (PNG bitmaps in `sbix` / `CBDT` are not decoded), Brotli (no WOFF2), bzip2 (`FT_Stream_OpenBzip2` reports `FT_Err_Unimplemented_Feature`), HarfBuzz in the autofitter, and ClearType-style LCD filtering. LCD rendering is the Harmony kind: `FT_Library_SetLcdGeometry` works, while `FT_Library_SetLcdFilter` reports `FT_Err_Unimplemented_Feature`.
- The validators `FT_OpenType_Validate`, `FT_TrueTypeGX_Validate` and `FT_ClassicKern_Validate` report `FT_Err_Unimplemented_Feature`, because the otvalid / gxvalid modules are not in upstream's default `modules.cfg`. `FT_Error_String` returns null (no `FT_CONFIG_OPTION_ERROR_STRINGS`), and the `ftlogging.h` functions do nothing (no `FT_DEBUG_LOGGING`).

`compat.freetype` gained the base extensions (stroker, gasp, Type 1 / CID / BDF / PFR / WinFNT queries, fsType, validation entry points, bzip2) in mcpp-index #589, at packaging revision 1 (#591). An older install refreshes on the next build.

## Licensing

FreeType is used under the FreeType License (FTL). Its advertising clause asks the documentation of a product built with it to credit *"Portions of this software are copyright © 2024 The FreeType Project (www.freetype.org). All rights reserved."*. The build deploys the licenses next to a program under `licenses/freetype/` ([NOTICE](../NOTICE)).

## Checks

- `mcpp test`:
  - `face_test`: the library, custom memory, modules and properties; faces from memory, files, `FT_Open_Args` and gzip / LZW streams; sizes, character maps and variation sequences; names, SFNT tables and flags; advances and kerning; variable fonts; PostScript, CID and BDF faces; errors.
  - `render_test`: every render mode; outlines; glyph objects; the stroker; bitmaps; synthesis; COLR v0 / v1 and CPAL; OT-SVG hooks; the cache; fixed-point and trigonometry; lists.
  - `macros`: constants and functions in constant expressions.
  - `header_mix_test`: the headers and the import together.
  - `exports_test`: the whole surface.

  The fonts are in [tests/data](../tests/data/README.md).
- `python3 tools/check_exports.py`: the generated files are fresh, every declaration is exported, and every macro is mapped or listed with its reason.
- `python3 tools/run_conformance.py`: FreeType's own test programs (`tests/issue-1063`, `src/tools/test_trig.c`, `src/tools/test_bbox.c`), ported onto the module in [conformance/](../conformance), with upstream's test font. Upstream's `src/tools/apinames.c`, compiled unchanged, lists the API from the headers, and the module must export every name it finds.
- `python3 tools/make_test_fonts.py` (fontTools 4.67.0) reproduces the test fonts byte for byte.
