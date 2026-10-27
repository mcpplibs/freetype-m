// The raw headers and the import in one translation unit (the side header
// first, the canonical order): the module's names are the very same
// entities; the macros a constant or function cannot be work beside them
// (include names, FT_ENC_TAG, FT_ERR_EQ, the error-table X-macros); the
// headers' own values equal the mapped constants. While a header's macro is
// defined it expands even inside a qualified name, so
// `macros::FT_LOAD_RENDER` is only reachable with the macro set aside; and
// FT_Err_* are the headers' enumerators here, so they are not brought in
// with a using-directive (it would make them ambiguous).
#include <mwrap/freetype/macros.hpp>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include <gtest/gtest.h>

import std;
import mcpplibs.mwrap.freetype;

namespace m = mcpplibs::mwrap::freetype::macros;

namespace {
constexpr long raw_render = FT_LOAD_RENDER, raw_kerning = FT_FACE_FLAG_KERNING;
constexpr FT_Tag raw_tag = FT_MAKE_TAG('k', 'e', 'r', 'n');

// An enumeration built with the header's FT_ENC_TAG, as FreeType builds FT_Encoding.
enum class Mine : FT_UInt32 { FT_ENC_TAG(latin, 'l', 'a', 't', 'n') };

// The error table, generated from fterrdef.h with the X-macros.
struct ErrorName {
    int code;
    const char* message;
};
#undef FTERRORS_H_
#define FT_ERRORDEF(e, v, s) {e, s},
#define FT_ERROR_START_LIST {
#define FT_ERROR_END_LIST {0, nullptr}};
constexpr ErrorName error_table[] =
#include FT_ERRORS_H
}  // namespace

#pragma push_macro("FT_LOAD_RENDER")
#pragma push_macro("FT_FACE_FLAG_KERNING")
#pragma push_macro("FT_MAKE_TAG")
#undef FT_LOAD_RENDER
#undef FT_FACE_FLAG_KERNING
#undef FT_MAKE_TAG
static_assert(m::FT_LOAD_RENDER == raw_render);
static_assert(m::FT_FACE_FLAG_KERNING == raw_kerning);
static_assert(m::FT_MAKE_TAG('k', 'e', 'r', 'n') == raw_tag);
static_assert(std::is_same_v<decltype(m::FT_LOAD_RENDER), decltype(raw_render)>);
#pragma pop_macro("FT_MAKE_TAG")
#pragma pop_macro("FT_FACE_FLAG_KERNING")
#pragma pop_macro("FT_LOAD_RENDER")

TEST(HeaderMix, SameEntitiesAndValues) {
    static_assert(std::is_same_v<decltype(&::FT_Init_FreeType), FT_Error (*)(FT_Library*)>);
    static_assert(std::is_same_v<FT_Face, ::FT_FaceRec_*>);
    static_assert(static_cast<FT_UInt32>(Mine::latin) == FT_MAKE_TAG('l', 'a', 't', 'n'));
    EXPECT_EQ(m::FT_Err_Invalid_Argument, ::FT_Err_Invalid_Argument);
    EXPECT_EQ(m::FT_Mod_Err_Max, ::FT_Mod_Err_Max);

    FT_Library lib{};
    ASSERT_EQ(FT_Init_FreeType(&lib), FT_Err_Ok);
    FT_Face face{};
    const std::vector<FT_Byte> garbage(4096, 0x5A);  // no format's signature
    FT_Error error = FT_New_Memory_Face(lib, garbage.data(), static_cast<FT_Long>(garbage.size()), 0, &face);
    // The token-pasting macros. fterrors.h undefines FT_ERR_PREFIX outside
    // the library's own build; a program that uses them names its prefix.
#define FT_ERR_PREFIX FT_Err_
    EXPECT_TRUE(FT_ERR_EQ(error, Unknown_File_Format));
    EXPECT_TRUE(FT_ERR_NEQ(error, Ok));
    EXPECT_EQ(FT_ERR(Unknown_File_Format), m::FT_Err_Unknown_File_Format);
#undef FT_ERR_PREFIX
    const auto* entry = std::find_if(std::begin(error_table), std::end(error_table),
                                     [&](const ErrorName& e) { return e.code == error; });
    ASSERT_NE(entry, std::end(error_table));
    EXPECT_STREQ(entry->message, "unknown file format");
    EXPECT_EQ(std::size(error_table), 96u);  // 95 codes and the terminator
    // A header macro and the mapped function agree on a real face.
    std::vector<FT_Byte> font;
    {
        std::ifstream in(std::filesystem::path(std::source_location::current().file_name()).parent_path() / "data" /
                             "mwrap-test.ttf",
                         std::ios::binary);
        font.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    ASSERT_EQ(FT_New_Memory_Face(lib, font.data(), static_cast<FT_Long>(font.size()), 0, &face), FT_Err_Ok);
    const bool raw_kerns = FT_HAS_KERNING(face), raw_color = FT_HAS_COLOR(face);
#pragma push_macro("FT_HAS_KERNING")
#pragma push_macro("FT_HAS_COLOR")
#undef FT_HAS_KERNING
#undef FT_HAS_COLOR
    EXPECT_EQ(m::FT_HAS_KERNING(face), raw_kerns);
    EXPECT_EQ(m::FT_HAS_COLOR(face), raw_color);
    EXPECT_TRUE(raw_kerns && raw_color);
#pragma pop_macro("FT_HAS_COLOR")
#pragma pop_macro("FT_HAS_KERNING")
    FT_Done_Face(face);
    FT_Done_FreeType(lib);
}
