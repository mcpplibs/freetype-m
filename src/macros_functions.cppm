// The computation macros of FreeType as functions, under the macros' names in
// mcpplibs::mwrap::freetype::macros. Each computes what its macro's expansion
// computes, with the type that expansion has in C++; the ones that only do
// arithmetic are constexpr (usable in constant expressions, as the macros
// are). An argument is evaluated once, as in every macro here.
module;
#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftcache.h>
#include <freetype/ftmodapi.h>

#undef FT_HAS_HORIZONTAL
#undef FT_HAS_VERTICAL
#undef FT_HAS_KERNING
#undef FT_IS_SCALABLE
#undef FT_IS_SFNT
#undef FT_IS_FIXED_WIDTH
#undef FT_HAS_FIXED_SIZES
#undef FT_HAS_FAST_GLYPHS
#undef FT_HAS_GLYPH_NAMES
#undef FT_HAS_MULTIPLE_MASTERS
#undef FT_IS_NAMED_INSTANCE
#undef FT_IS_VARIATION
#undef FT_IS_CID_KEYED
#undef FT_IS_TRICKY
#undef FT_HAS_COLOR
#undef FT_HAS_SVG
#undef FT_HAS_SBIX
#undef FT_HAS_SBIX_OVERLAY

#undef FT_LOAD_TARGET_
#undef FT_LOAD_TARGET_MODE
#undef FT_MAKE_TAG
#undef FT_IS_EMPTY
#undef FT_BOOL
#undef FT_ERROR_BASE
#undef FT_ERROR_MODULE
#undef FT_CURVE_TAG
#undef FTC_IMAGE_TYPE_COMPARE
#undef FT_FACE_DRIVER_NAME

export module mcpplibs.mwrap.freetype.macros:functions;

export namespace mcpplibs::mwrap::freetype::macros {

// freetype.h: the face flags of a face, `( !!( (face)->face_flags & FLAG ) )`.
constexpr bool FT_HAS_HORIZONTAL(const FT_FaceRec* face) noexcept { return !!(face->face_flags & FT_FACE_FLAG_HORIZONTAL); }
constexpr bool FT_HAS_VERTICAL(const FT_FaceRec* face) noexcept { return !!(face->face_flags & FT_FACE_FLAG_VERTICAL); }
constexpr bool FT_HAS_KERNING(const FT_FaceRec* face) noexcept { return !!(face->face_flags & FT_FACE_FLAG_KERNING); }
constexpr bool FT_IS_SCALABLE(const FT_FaceRec* face) noexcept { return !!(face->face_flags & FT_FACE_FLAG_SCALABLE); }
constexpr bool FT_IS_SFNT(const FT_FaceRec* face) noexcept { return !!(face->face_flags & FT_FACE_FLAG_SFNT); }
constexpr bool FT_IS_FIXED_WIDTH(const FT_FaceRec* face) noexcept { return !!(face->face_flags & FT_FACE_FLAG_FIXED_WIDTH); }
constexpr bool FT_HAS_FIXED_SIZES(const FT_FaceRec* face) noexcept { return !!(face->face_flags & FT_FACE_FLAG_FIXED_SIZES); }
// `0`: deprecated upstream, always false; the argument is not read.
constexpr int FT_HAS_FAST_GLYPHS(const FT_FaceRec*) noexcept { return 0; }
constexpr bool FT_HAS_GLYPH_NAMES(const FT_FaceRec* face) noexcept { return !!(face->face_flags & FT_FACE_FLAG_GLYPH_NAMES); }
constexpr bool FT_HAS_MULTIPLE_MASTERS(const FT_FaceRec* face) noexcept {
    return !!(face->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS);
}
constexpr bool FT_IS_NAMED_INSTANCE(const FT_FaceRec* face) noexcept { return !!(face->face_index & 0x7FFF0000L); }
constexpr bool FT_IS_VARIATION(const FT_FaceRec* face) noexcept { return !!(face->face_flags & FT_FACE_FLAG_VARIATION); }
constexpr bool FT_IS_CID_KEYED(const FT_FaceRec* face) noexcept { return !!(face->face_flags & FT_FACE_FLAG_CID_KEYED); }
constexpr bool FT_IS_TRICKY(const FT_FaceRec* face) noexcept { return !!(face->face_flags & FT_FACE_FLAG_TRICKY); }
constexpr bool FT_HAS_COLOR(const FT_FaceRec* face) noexcept { return !!(face->face_flags & FT_FACE_FLAG_COLOR); }
constexpr bool FT_HAS_SVG(const FT_FaceRec* face) noexcept { return !!(face->face_flags & FT_FACE_FLAG_SVG); }
constexpr bool FT_HAS_SBIX(const FT_FaceRec* face) noexcept { return !!(face->face_flags & FT_FACE_FLAG_SBIX); }
constexpr bool FT_HAS_SBIX_OVERLAY(const FT_FaceRec* face) noexcept { return !!(face->face_flags & FT_FACE_FLAG_SBIX_OVERLAY); }

// freetype.h: `( FT_STATIC_CAST( FT_Int32, (x) & 15 ) << 16 )`.
constexpr auto FT_LOAD_TARGET_(auto x) noexcept { return static_cast<FT_Int32>(x & 15) << 16; }
// freetype.h: `FT_STATIC_CAST( FT_Render_Mode, ( (x) >> 16 ) & 15 )`.
constexpr FT_Render_Mode FT_LOAD_TARGET_MODE(auto x) noexcept { return static_cast<FT_Render_Mode>((x >> 16) & 15); }

// fttypes.h: each argument's low byte, big-endian, as an FT_Tag.
constexpr FT_Tag FT_MAKE_TAG(auto x1, auto x2, auto x3, auto x4) noexcept {
    return (static_cast<FT_Tag>(static_cast<unsigned char>(x1)) << 24) |
           (static_cast<FT_Tag>(static_cast<unsigned char>(x2)) << 16) |
           (static_cast<FT_Tag>(static_cast<unsigned char>(x3)) << 8) |
           static_cast<FT_Tag>(static_cast<unsigned char>(x4));
}
// fttypes.h: `( (list).head == 0 )`, of a list record.
constexpr bool FT_IS_EMPTY(const FT_ListRec& list) noexcept { return list.head == nullptr; }
// fttypes.h: `FT_STATIC_CAST( FT_Bool, (x) != 0 )`.
constexpr FT_Bool FT_BOOL(auto x) noexcept { return static_cast<FT_Bool>(x != 0); }
// fttypes.h: the generic error of an error code, and its module.
constexpr auto FT_ERROR_BASE(auto x) noexcept { return x & 0xFF; }
constexpr auto FT_ERROR_MODULE(auto x) noexcept { return x & 0xFF00U; }

// ftimage.h: `( flag & 0x03 )`, the on/conic/cubic bits of an outline tag.
constexpr auto FT_CURVE_TAG(auto flag) noexcept { return flag & 0x03; }

// ftcache.h: two image types select the same images.
constexpr bool FTC_IMAGE_TYPE_COMPARE(const FTC_ImageTypeRec* d1, const FTC_ImageTypeRec* d2) noexcept {
    return d1->face_id == d2->face_id && d1->width == d2->width && d1->flags == d2->flags;
}

// ftmodapi.h: the name of the driver of a face (a valid module name for
// FT_Property_Set), read through the driver's module class as the macro does.
inline const FT_String* FT_FACE_DRIVER_NAME(const FT_FaceRec* face) noexcept {
    return (*reinterpret_cast<FT_Module_Class**>(face->driver))->module_name;
}

}  // namespace mcpplibs::mwrap::freetype::macros
