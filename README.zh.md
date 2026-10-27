# freetype

> 通过 C++23 模块使用 FreeType 2（可移植的字体引擎）。

[English](README.md) · **简体中文** · [上游](https://gitlab.freedesktop.org/freetype/freetype) · [发布](https://github.com/mcpplibs/freetype-m/releases) · [mcpp](https://github.com/mcpp-community/mcpp) · [索引](https://github.com/mcpplibs/mcpp-index)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![FreeType](https://img.shields.io/badge/FreeType-FTL-blue.svg)](upstream/freetype/docs/FTL.TXT)
[![mcpp-community](https://img.shields.io/badge/mcpp--community-mwrap-blue)](https://github.com/mcpp-community)
[![CI](https://github.com/mcpplibs/freetype-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/mcpplibs/freetype-m/actions/workflows/ci.yml)

上游的 C API 保持不变。`FT_Library`、`FT_New_Memory_Face`、`FT_Load_Glyph`、
`FT_Outline_Decompose`、`FT_Stroker_New` 就是 FreeType 公开头文件里的上游全局名字，
覆盖 `<ft2build.h>` 列出的每一个头文件：从 `freetype.h` 到缓存、描边、多主字体、
彩色字形和 SFNT 表的 API。库本身是 `compat.freetype` 2.13.3。宏映射成常量和
constexpr 函数。一项审计对照发布版的头文件，逐一检查每个声明和每个宏。
FreeType 自带的测试程序移植到本模块之后，每次 CI 都全部通过。

## 模块

- `import mcpplibs.mwrap.freetype;`：完整 API（218 个函数，以及全部类型和枚举值），并重新导出下面这个模块
- `import mcpplibs.mwrap.freetype.macros;`：`FT_LOAD_RENDER`、`FT_FACE_FLAG_KERNING`、`FT_Err_Ok`、`FT_HAS_KERNING`、`FT_MAKE_TAG` 等，位于 `mcpplibs::mwrap::freetype::macros`

> 更多模块 API -> [用法和 API](docs/usage.md)

## 快速开始

```sh
mcpp new banner --template mcpplibs.mwrap.freetype@2.13.3
cd banner
mcpp run                                   # 光栅化一颗星和一个描边的圆环
mcpp run -- /path/to/font.ttf "Hello"      # 用任意字体排一行文字，带字距调整
```

默认模板逐点构造一个轮廓，再在它外面描边出一个圆环。两者都由 FreeType
光栅化，然后打印在终端上。如果给出字体文件，程序改为按这个字体的
步进宽度和字距排出一行文字。

```toml
[dependencies]
"mcpplibs.mwrap.freetype" = "2.13.3"
```

```cpp
import std;
import mcpplibs.mwrap.freetype;
using namespace mcpplibs::mwrap::freetype::macros;  // FT_LOAD_RENDER、FT_Err_Ok …

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

## 宏

具名模块不能导出宏。FreeType 的 1274 个宏按下面的方式映射：

- 1013 个常量，每个都保留宏原来的类型。
- 头文件里属于无名枚举的 `FT_Err_*` 错误码，映射成 `FT_Error` 常量。
- 22 个类型别名。
- 28 个函数：`FT_HAS_*` / `FT_IS_*`、`FT_LOAD_TARGET_`、`FT_LOAD_TARGET_MODE`、
  `FT_MAKE_TAG`、`FT_CURVE_TAG` 等。

常量和函数都表达不了的宏保留在 `<mwrap/freetype/macros.hpp>` 里，需要在
import 之前包含：头文件名宏（`FT_FREETYPE_H` 等）、`FT_ENC_TAG`、做记号拼接的
`FT_ERR_EQ`、生成错误表的 X 宏，以及配置开关。库的编译配置、为什么不包括
Carbon 头文件 `ftmac.h`、怎样把头文件和 import 混用，见
[docs/usage.md](docs/usage.md)。

## 示例

- [examples/ftdump](examples/ftdump) 描述字体文件：字体面、格式和驱动、标志、
  度量、字符映射表、可变轴、名字、表、调色板、PostScript 和 BDF 属性。
- [conformance](conformance)：FreeType 的 `tests/issue-1063`、
  `src/tools/test_trig.c` 和 `src/tools/test_bbox.c`，移植到本模块上。
  [tools/run_conformance.py](tools/run_conformance.py) 用上游指定的测试字体运行
  它们，拿精确值核对包围盒，并用上游的 `apinames` 扫描头文件、对照导出列表。

## 构建和测试

Linux（GCC 和 Clang）、macOS、Windows 的 CI 先构建模块，然后运行：

- 测试：在真实字体和专门制作的字体上测试 API（各种渲染模式、轮廓、描边、位图、
  彩色字形、SVG 钩子、缓存、可变字体、字距、压缩流）、宏、头文件与 import 混用，
  以及通过 import 访问整个导出接口；
- 导出审计（[tools/check_exports.py](tools/check_exports.py)）；
- FreeType 自带的测试；
- ftdump。

另有一个 job 把默认模板当作已安装的包生成项目并运行。

## 许可证

封装部分采用 [Apache-2.0](LICENSE)，作者为 mcpp-community（[NOTICE](NOTICE)）。
FreeType 保留它自己的许可证：本包按 [LICENSE.TXT](upstream/freetype/LICENSE.TXT)
选择 FreeType License（[upstream/freetype/docs/FTL.TXT](upstream/freetype/docs/FTL.TXT)），
BDF 和 PCF 驱动的声明在 [upstream/freetype/src](upstream/freetype/src)。
Portions of this software are copyright © 2024 The FreeType Project
(www.freetype.org). All rights reserved.
