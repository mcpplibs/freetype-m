// The macro mappings in constant expressions, through the macros module
// (no header): constants keep their macro's value and type, the
// computation macros are constexpr functions with the macros' results.
import std;
import mcpplibs.mwrap.freetype;  // re-exports mcpplibs.mwrap.freetype.macros

namespace m = mcpplibs::mwrap::freetype::macros;

// Version and flags, each of its macro's type (1L << n is a long).
static_assert(m::FREETYPE_MAJOR == 2 && m::FREETYPE_MINOR == 13 && m::FREETYPE_PATCH == 3);
static_assert(std::is_same_v<decltype(m::FT_FACE_FLAG_KERNING), const long>);
static_assert(m::FT_FACE_FLAG_SCALABLE == (1L << 0) && m::FT_FACE_FLAG_KERNING == (1L << 6));
static_assert(m::FT_STYLE_FLAG_ITALIC == 1 && m::FT_STYLE_FLAG_BOLD == 2);
static_assert(m::FT_LOAD_DEFAULT == 0 && m::FT_LOAD_NO_SCALE == (1L << 0) && m::FT_LOAD_RENDER == (1L << 2));
static_assert(m::FT_LOAD_COLOR == (1L << 20) && m::FT_LOAD_NO_SVG == (1L << 24));
static_assert(m::FT_OPEN_MEMORY == 0x1 && m::FT_OPEN_STREAM == 0x2 && m::FT_OPEN_PARAMS == 0x10);
static_assert(m::FT_OUTLINE_EVEN_ODD_FILL == 0x2 && m::FT_OUTLINE_HIGH_PRECISION == 0x100);
static_assert(m::FT_CURVE_TAG_ON == 0x01 && m::FT_CURVE_TAG_CONIC == 0x00 && m::FT_CURVE_TAG_CUBIC == 0x02);
static_assert(m::FT_RASTER_FLAG_AA == 0x1 && m::FT_RASTER_FLAG_DIRECT == 0x2);
static_assert(m::FT_FSTYPE_EDITABLE_EMBEDDING == 0x0008);

// The load-target computation and its inverse.
static_assert(m::FT_LOAD_TARGET_(FT_RENDER_MODE_MONO) == (2 << 16));
static_assert(m::FT_LOAD_TARGET_NORMAL == 0 && m::FT_LOAD_TARGET_LIGHT == (1 << 16));
static_assert(m::FT_LOAD_TARGET_LCD_V == (4 << 16));
static_assert(m::FT_LOAD_TARGET_MODE(m::FT_LOAD_TARGET_LCD | m::FT_LOAD_RENDER) == FT_RENDER_MODE_LCD);
static_assert(std::is_same_v<decltype(m::FT_LOAD_TARGET_MODE(0)), FT_Render_Mode>);

// Tags.
static_assert(m::FT_MAKE_TAG('g', 'l', 'y', 'f') == 0x676C7966u);
static_assert(m::FT_MAKE_TAG(0x1FF, 'a', 'b', 'c') == 0xFF616263u);  // each argument's low byte
static_assert(std::is_same_v<decltype(m::FT_MAKE_TAG('a', 'b', 'c', 'd')), FT_Tag>);
static_assert(m::TTAG_glyf == m::FT_MAKE_TAG('g', 'l', 'y', 'f') && m::TTAG_OS2 == m::FT_MAKE_TAG('O', 'S', '/', '2'));
static_assert(m::FT_PARAM_TAG_STEM_DARKENING == m::FT_MAKE_TAG('d', 'a', 'r', 'k'));
static_assert(m::FT_PARAM_TAG_IGNORE_PREFERRED_FAMILY == m::FT_PARAM_TAG_IGNORE_TYPOGRAPHIC_FAMILY);
static_assert(FT_ENCODING_UNICODE == m::FT_MAKE_TAG('u', 'n', 'i', 'c'));
static_assert(FT_GLYPH_FORMAT_OUTLINE == m::FT_MAKE_TAG('o', 'u', 't', 'l'));

// Outline tags, booleans, error codes.
static_assert(m::FT_CURVE_TAG(0x01 | 0x08) == m::FT_CURVE_TAG_ON);
static_assert(m::FT_CURVE_TAG(0x02 | 0x10) == m::FT_CURVE_TAG_CUBIC);
static_assert(m::FT_BOOL(42) == 1 && m::FT_BOOL(0) == 0 && std::is_same_v<decltype(m::FT_BOOL(1)), FT_Bool>);
static_assert(m::FT_ERROR_BASE(0x0123) == 0x23 && m::FT_ERROR_MODULE(0x0123) == 0x0100u);
static_assert(m::FT_Err_Ok == 0 && m::FT_Err_Out_Of_Memory == 0x40 && m::FT_Err_Invalid_Argument == 0x06);
static_assert(m::FT_Mod_Err_Base == 0 && m::FT_Mod_Err_Max > m::FT_Mod_Err_Base);
static_assert(std::is_same_v<decltype(m::FT_Err_Ok), const FT_Error>);

// The legacy spellings, and the platform's integer type macros.
static_assert(m::ft_encoding_unicode == FT_ENCODING_UNICODE && m::ft_render_mode_mono == FT_RENDER_MODE_MONO);
static_assert(m::ft_kerning_unscaled == FT_KERNING_UNSCALED && m::ft_glyph_bbox_pixels == FT_GLYPH_BBOX_PIXELS);
static_assert(m::ft_open_memory == m::FT_OPEN_MEMORY && m::FT_Curve_Tag_On == m::FT_CURVE_TAG_ON);
static_assert(std::is_same_v<m::FT_Outline_MoveTo_Func, FT_Outline_MoveToFunc>);
static_assert(std::is_same_v<m::FT_Raster_Span_Func, FT_SpanFunc>);
static_assert(std::is_same_v<m::FT_INT64, FT_Int64> && sizeof(m::FT_UINT64) == 8);

// SFNT identifiers and trigonometry constants.
static_assert(m::TT_PLATFORM_MICROSOFT == 3 && m::TT_MS_ID_UNICODE_CS == 1 && m::TT_NAME_ID_FONT_FAMILY == 1);
static_assert(m::TT_MS_LANGID_ENGLISH_UNITED_STATES == 0x0409 && m::TT_MS_ID_GB2312 == m::TT_MS_ID_PRC);
static_assert(m::FT_ANGLE_PI == 180L << 16 && m::FT_ANGLE_2PI == 2 * m::FT_ANGLE_PI && m::FT_ANGLE_PI4 == 45L << 16);
static_assert(m::FT_VALIDATE_feat == m::FT_VALIDATE_GX_START << m::FT_VALIDATE_feat_INDEX);

// Face predicates over a constant face record.
constexpr FT_FaceRec face_with(FT_Long flags, FT_Long index) {
    FT_FaceRec face{};
    face.face_flags = flags;
    face.face_index = index;
    return face;
}
constexpr FT_FaceRec kerned = face_with(m::FT_FACE_FLAG_KERNING | m::FT_FACE_FLAG_SCALABLE, 3 << 16);
static_assert(m::FT_HAS_KERNING(&kerned) && m::FT_IS_SCALABLE(&kerned) && !m::FT_HAS_COLOR(&kerned));
static_assert(m::FT_IS_NAMED_INSTANCE(&kerned) && m::FT_HAS_FAST_GLYPHS(&kerned) == 0);
constexpr FTC_ImageTypeRec a{nullptr, 12, 12, 0}, b{nullptr, 12, 12, 0}, c{nullptr, 13, 12, 0};
static_assert(m::FTC_IMAGE_TYPE_COMPARE(&a, &b) && !m::FTC_IMAGE_TYPE_COMPARE(&a, &c));
constexpr FT_ListRec empty_list{};
static_assert(m::FT_IS_EMPTY(empty_list));

int main() { return m::FT_LOAD_TARGET_MODE(m::FT_LOAD_TARGET_MONO) == FT_RENDER_MODE_MONO ? 0 : 1; }
