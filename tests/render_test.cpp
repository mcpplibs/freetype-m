// Glyphs through import mcpplibs.mwrap.freetype: loading and rendering in
// every mode, outlines (decomposition, boxes, transforms, emboldening,
// rasterisation into spans), glyph objects, the stroker, bitmap conversion
// and blending, synthesis, colour glyphs (COLR v0 layers, the COLR v1 paint
// graph, CPAL palettes), OT-SVG through renderer hooks, the cache
// subsystem, fixed-point and trigonometric arithmetic, lists.
#include <gtest/gtest.h>

import std;
import mcpplibs.mwrap.freetype;

using namespace mcpplibs::mwrap::freetype::macros;

namespace {

std::filesystem::path data(std::string_view name) {
    return std::filesystem::path(std::source_location::current().file_name()).parent_path() / "data" / name;
}

std::vector<FT_Byte> bytes(std::string_view name) {
    std::ifstream in(data(name), std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

struct Library {
    FT_Library lib{};
    Library() { EXPECT_EQ(FT_Init_FreeType(&lib), FT_Err_Ok); }
    ~Library() { FT_Done_FreeType(lib); }
};

struct Face {
    std::vector<FT_Byte> buffer;
    FT_Face face{};
    Face(FT_Library lib, std::string_view name) : buffer(bytes(name)) {
        EXPECT_EQ(FT_New_Memory_Face(lib, buffer.data(), static_cast<FT_Long>(buffer.size()), 0, &face), FT_Err_Ok)
            << name;
    }
    ~Face() {
        if (face)
            FT_Done_Face(face);
    }
    FT_Face operator->() const { return face; }
};

// The test font's glyph order (tools/make_test_fonts.py).
constexpr FT_UInt gid_A = 2, gid_o = 4, gid_heart = 5, gid_heart_inner = 6, gid_star = 7, gid_svgsquare = 10;

struct Segments {
    int move = 0, line = 0, conic = 0, cubic = 0;
};

Segments decompose(const FT_Outline& outline) {
    FT_Outline_Funcs funcs{};
    funcs.move_to = [](const FT_Vector*, void* u) { return ++static_cast<Segments*>(u)->move, 0; };
    funcs.line_to = [](const FT_Vector*, void* u) { return ++static_cast<Segments*>(u)->line, 0; };
    funcs.conic_to = [](const FT_Vector*, const FT_Vector*, void* u) { return ++static_cast<Segments*>(u)->conic, 0; };
    funcs.cubic_to = [](const FT_Vector*, const FT_Vector*, const FT_Vector*, void* u) {
        return ++static_cast<Segments*>(u)->cubic, 0;
    };
    Segments s;
    EXPECT_EQ(FT_Outline_Decompose(const_cast<FT_Outline*>(&outline), &funcs, &s), FT_Err_Ok);
    return s;
}

// A square outline, (x, y) to (x + side, y + side) in 26.6, counter-clockwise.
struct Square {
    FT_Vector points[4];
    unsigned char tags[4] = {FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON};
    unsigned short contours[1] = {3};
    FT_Outline outline{};
    Square(FT_Pos x, FT_Pos y, FT_Pos side) : points{{x, y}, {x + side, y}, {x + side, y + side}, {x, y + side}} {
        outline.n_contours = 1;
        outline.n_points = 4;
        outline.points = points;
        outline.tags = tags;
        outline.contours = contours;
        outline.flags = FT_OUTLINE_NONE;
    }
};

std::uint64_t coverage_sum(const FT_Bitmap& bitmap) {
    std::uint64_t sum = 0;
    for (unsigned r = 0; r < bitmap.rows; ++r)
        for (unsigned c = 0; c < bitmap.width; ++c)
            sum += bitmap.buffer[r * bitmap.pitch + c];
    return sum;
}

}  // namespace

TEST(Render, EveryMode) {
    Library l;
    Face jb(l.lib, "JetBrainsMono.ttf");
    ASSERT_EQ(FT_Set_Pixel_Sizes(jb.face, 0, 32), FT_Err_Ok);
    FT_GlyphSlot slot = jb->glyph;

    ASSERT_EQ(FT_Load_Char(jb.face, 'A', FT_LOAD_RENDER), FT_Err_Ok);
    EXPECT_EQ(slot->format, FT_GLYPH_FORMAT_BITMAP);
    EXPECT_EQ(slot->bitmap.pixel_mode, FT_PIXEL_MODE_GRAY);
    EXPECT_EQ(slot->bitmap.num_grays, 256);
    EXPECT_GT(slot->bitmap.width, 10u);
    EXPECT_GT(slot->bitmap.rows, 15u);
    EXPECT_GT(coverage_sum(slot->bitmap), 0u);
    EXPECT_EQ(slot->advance.x % 64, 0);  // hinted: whole pixels
    EXPECT_NEAR(slot->linearHoriAdvance, 19.2 * 65536, 8);  // 600 units at 32 px, unhinted, 16.16

    ASSERT_EQ(FT_Load_Char(jb.face, 'A', FT_LOAD_RENDER | FT_LOAD_TARGET_MONO), FT_Err_Ok);
    EXPECT_EQ(slot->bitmap.pixel_mode, FT_PIXEL_MODE_MONO);
    EXPECT_EQ(FT_LOAD_TARGET_MODE(FT_LOAD_TARGET_MONO), FT_RENDER_MODE_MONO);

    ASSERT_EQ(FT_Load_Char(jb.face, 'A', FT_LOAD_TARGET_LIGHT), FT_Err_Ok);
    EXPECT_EQ(slot->format, FT_GLYPH_FORMAT_OUTLINE);
    ASSERT_EQ(FT_Render_Glyph(slot, FT_RENDER_MODE_LIGHT), FT_Err_Ok);
    EXPECT_EQ(slot->bitmap.pixel_mode, FT_PIXEL_MODE_GRAY);

    // Subpixel rendering is the Harmony kind here (upstream's default
    // ftoption.h): a geometry, not a filter.
    EXPECT_EQ(FT_Library_SetLcdFilter(l.lib, FT_LCD_FILTER_DEFAULT), FT_Err_Unimplemented_Feature);
    unsigned char weights[FT_LCD_FILTER_FIVE_TAPS] = {0x08, 0x4D, 0x56, 0x4D, 0x08};
    EXPECT_EQ(FT_Library_SetLcdFilterWeights(l.lib, weights), FT_Err_Unimplemented_Feature);
    FT_Vector geometry[3] = {{-21, 0}, {0, 0}, {21, 0}};
    EXPECT_EQ(FT_Library_SetLcdGeometry(l.lib, geometry), FT_Err_Ok);
    ASSERT_EQ(FT_Load_Char(jb.face, 'A', FT_LOAD_TARGET_LCD), FT_Err_Ok);
    ASSERT_EQ(FT_Render_Glyph(slot, FT_RENDER_MODE_LCD), FT_Err_Ok);
    EXPECT_EQ(slot->bitmap.pixel_mode, FT_PIXEL_MODE_LCD);
    EXPECT_EQ(slot->bitmap.width % 3, 0u);
    ASSERT_EQ(FT_Load_Char(jb.face, 'A', FT_LOAD_TARGET_LCD_V), FT_Err_Ok);
    ASSERT_EQ(FT_Render_Glyph(slot, FT_RENDER_MODE_LCD_V), FT_Err_Ok);
    EXPECT_EQ(slot->bitmap.pixel_mode, FT_PIXEL_MODE_LCD_V);
    EXPECT_EQ(slot->bitmap.rows % 3, 0u);

    // Signed distance fields.
    ASSERT_EQ(FT_Load_Char(jb.face, 'o', FT_LOAD_DEFAULT), FT_Err_Ok);
    ASSERT_EQ(FT_Render_Glyph(slot, FT_RENDER_MODE_SDF), FT_Err_Ok);
    EXPECT_EQ(slot->bitmap.pixel_mode, FT_PIXEL_MODE_GRAY);
    EXPECT_GT(slot->bitmap.width, 0u);

    // Unhinted, unscaled, no bitmap: font units in the outline.
    ASSERT_EQ(FT_Load_Char(jb.face, 'A', FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP), FT_Err_Ok);
    EXPECT_EQ(slot->metrics.horiAdvance, 600);
    FT_BBox box{};
    FT_Outline_Get_CBox(&slot->outline, &box);
    EXPECT_EQ(box.xMin, 50);
    EXPECT_EQ(box.xMax, 550);
    EXPECT_EQ(box.yMax, 730);

    // A composite glyph, unexpanded: its subglyphs.
    ASSERT_EQ(FT_Load_Char(jb.face, 0xC1, FT_LOAD_NO_SCALE | FT_LOAD_NO_RECURSE), FT_Err_Ok);  // Á
    EXPECT_EQ(slot->format, FT_GLYPH_FORMAT_COMPOSITE);
    ASSERT_EQ(slot->num_subglyphs, 2u);
    FT_Int index = 0, arg1 = 0, arg2 = 0;
    FT_UInt flags = 0;
    FT_Matrix transform{};
    ASSERT_EQ(FT_Get_SubGlyph_Info(slot, 0, &index, &flags, &arg1, &arg2, &transform), FT_Err_Ok);
    EXPECT_EQ(static_cast<FT_UInt>(index), FT_Get_Char_Index(jb.face, 'A'));
    EXPECT_NE(flags & FT_SUBGLYPH_FLAG_ARGS_ARE_XY_VALUES, 0u);
    EXPECT_EQ(transform.xx, 0x10000);
    EXPECT_EQ(FT_Get_SubGlyph_Info(slot, 2, &index, &flags, &arg1, &arg2, &transform), FT_Err_Invalid_Argument);

    // Bytecode hinting goes through the TrueType interpreter: a debug hook
    // sees every program run (here instead of running it).
    static int runs = 0;
    runs = 0;
    FT_Set_Debug_Hook(l.lib, FT_DEBUG_HOOK_TRUETYPE, [](void*) -> FT_Error { return ++runs, 0; });
    Face hooked(l.lib, "JetBrainsMono.ttf");
    FT_Set_Pixel_Sizes(hooked.face, 0, 15);
    FT_Load_Char(hooked.face, 'a', FT_LOAD_DEFAULT);
    EXPECT_GT(runs, 0);
    FT_Set_Debug_Hook(l.lib, FT_DEBUG_HOOK_TRUETYPE, nullptr);
}

TEST(Outline, DecomposeAndMeasure) {
    Library l;
    Face ttf(l.lib, "mwrap-test.ttf"), cff(l.lib, "mwrap-test.otf");
    ASSERT_EQ(FT_Load_Glyph(ttf.face, gid_A, FT_LOAD_NO_SCALE), FT_Err_Ok);
    const FT_Outline& a = ttf->glyph->outline;
    EXPECT_EQ(a.n_contours, 1);
    EXPECT_EQ(a.n_points, 7);
    EXPECT_EQ(FT_Outline_Check(const_cast<FT_Outline*>(&a)), FT_Err_Ok);
    Segments s = decompose(a);
    EXPECT_EQ(s.move, 1);
    EXPECT_EQ(s.line, 7);  // six edges and the closing one
    FT_BBox box{};
    FT_Outline_Get_CBox(&a, &box);
    EXPECT_EQ(std::tuple(box.xMin, box.yMin, box.xMax, box.yMax), std::tuple(0, 0, 600, 700));
    EXPECT_EQ(FT_Outline_Get_BBox(const_cast<FT_Outline*>(&a), &box), FT_Err_Ok);
    EXPECT_EQ(std::tuple(box.xMin, box.yMin, box.xMax, box.yMax), std::tuple(0, 0, 600, 700));
    for (int i = 0; i < a.n_points; ++i)
        EXPECT_EQ(FT_CURVE_TAG(a.tags[i]), FT_CURVE_TAG_ON);

    ASSERT_EQ(FT_Load_Glyph(ttf.face, gid_o, FT_LOAD_NO_SCALE), FT_Err_Ok);
    s = decompose(ttf->glyph->outline);
    EXPECT_EQ(s.conic, 4);  // TrueType: quadratic arcs
    EXPECT_EQ(s.cubic, 0);
    // The arcs' extremes are on-curve points: the exact box.
    EXPECT_EQ(FT_Outline_Get_BBox(&ttf->glyph->outline, &box), FT_Err_Ok);
    EXPECT_EQ(std::tuple(box.xMin, box.yMin, box.xMax, box.yMax), std::tuple(50, 0, 550, 500));

    ASSERT_EQ(FT_Load_Glyph(cff.face, gid_o, FT_LOAD_NO_SCALE), FT_Err_Ok);
    s = decompose(cff->glyph->outline);
    EXPECT_EQ(s.cubic, 4);  // CFF: cubic arcs
    EXPECT_EQ(s.conic, 0);

    // The orientation of TrueType contours, reversed.
    Face jb(l.lib, "JetBrainsMono.ttf");
    ASSERT_EQ(FT_Load_Char(jb.face, 'o', FT_LOAD_NO_SCALE), FT_Err_Ok);
    EXPECT_EQ(FT_Outline_Get_Orientation(&jb->glyph->outline), FT_ORIENTATION_TRUETYPE);
    FT_Outline_Reverse(&jb->glyph->outline);
    EXPECT_EQ(FT_Outline_Get_Orientation(&jb->glyph->outline), FT_ORIENTATION_POSTSCRIPT);
    EXPECT_EQ(FT_Outline_GetInsideBorder(&jb->glyph->outline), FT_STROKER_BORDER_LEFT);
    EXPECT_EQ(FT_Outline_GetOutsideBorder(&jb->glyph->outline), FT_STROKER_BORDER_RIGHT);
}

TEST(Outline, TransformEmboldenAndRasterise) {
    Library l;
    Square square(0, 0, 10 * 64);
    FT_Outline outline{};
    ASSERT_EQ(FT_Outline_New(l.lib, 4, 1, &outline), FT_Err_Ok);
    ASSERT_EQ(FT_Outline_Copy(&square.outline, &outline), FT_Err_Ok);
    EXPECT_EQ(FT_Outline_Get_Orientation(&outline), FT_ORIENTATION_POSTSCRIPT);

    FT_Outline_Translate(&outline, 64, 128);
    FT_BBox box{};
    FT_Outline_Get_CBox(&outline, &box);
    EXPECT_EQ(std::tuple(box.xMin, box.yMin, box.xMax, box.yMax), std::tuple(64, 128, 704, 768));
    FT_Matrix twice{0x20000, 0, 0, 0x20000};
    FT_Outline_Transform(&outline, &twice);
    FT_Outline_Get_CBox(&outline, &box);
    EXPECT_EQ(std::tuple(box.xMin, box.yMin, box.xMax, box.yMax), std::tuple(128, 256, 1408, 1536));
    // Emboldening keeps the left and bottom edges and grows the outline by
    // the strength to the right and up.
    ASSERT_EQ(FT_Outline_Embolden(&outline, 64), FT_Err_Ok);
    FT_Outline_Get_CBox(&outline, &box);
    EXPECT_EQ(std::tuple(box.xMin, box.yMin, box.xMax, box.yMax), std::tuple(128, 256, 1472, 1600));
    ASSERT_EQ(FT_Outline_EmboldenXY(&outline, 64, 0), FT_Err_Ok);
    FT_Outline_Get_CBox(&outline, &box);
    EXPECT_EQ(std::tuple(box.xMin, box.yMin, box.xMax, box.yMax), std::tuple(128, 256, 1536, 1600));
    EXPECT_EQ(FT_Outline_Done(l.lib, &outline), FT_Err_Ok);

    // Into a caller's bitmap: a whole-pixel square covers 100 pixels fully.
    Square px(64, 64, 10 * 64);
    std::vector<unsigned char> buffer(16 * 16);
    FT_Bitmap target{};
    target.rows = target.width = 16;
    target.pitch = 16;
    target.buffer = buffer.data();
    target.num_grays = 256;
    target.pixel_mode = FT_PIXEL_MODE_GRAY;
    ASSERT_EQ(FT_Outline_Get_Bitmap(l.lib, &px.outline, &target), FT_Err_Ok);
    EXPECT_EQ(coverage_sum(target), 100u * 255u);
    EXPECT_EQ(buffer[15 * 16 + 1], 0);  // row 0 is the top: y in [15, 16) is outside
    EXPECT_EQ(buffer[14 * 16 + 1], 255);

    // Direct rendering into spans, with a half-pixel offset: the same area.
    Square half(32, 64, 10 * 64);
    struct Total {
        std::uint64_t area = 0;
        int calls = 0;
    } total;
    FT_Raster_Params params{};
    params.source = &half.outline;
    params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_CLIP;
    params.gray_spans = [](int, int count, const FT_Span* spans, void* user) {
        auto* t = static_cast<Total*>(user);
        ++t->calls;
        for (int i = 0; i < count; ++i)
            t->area += static_cast<std::uint64_t>(spans[i].len) * spans[i].coverage;
    };
    params.user = &total;
    params.clip_box = {0, 0, 32, 32};
    ASSERT_EQ(FT_Outline_Render(l.lib, &half.outline, &params), FT_Err_Ok);
    EXPECT_GE(total.calls, 10);
    // 100 px², the two half-covered columns at about half coverage.
    EXPECT_NEAR(static_cast<double>(total.area), 100.0 * 255.0, 10.0 * 2.0);
}

TEST(Glyph, ObjectsTransformsAndStroking) {
    Library l;
    Face t(l.lib, "mwrap-test.ttf");
    ASSERT_EQ(FT_Set_Pixel_Sizes(t.face, 0, 50), FT_Err_Ok);  // 1 unit = 0.05 px
    ASSERT_EQ(FT_Load_Glyph(t.face, gid_A, FT_LOAD_NO_HINTING), FT_Err_Ok);
    FT_Glyph glyph{};
    ASSERT_EQ(FT_Get_Glyph(t->glyph, &glyph), FT_Err_Ok);
    EXPECT_EQ(glyph->format, FT_GLYPH_FORMAT_OUTLINE);
    EXPECT_EQ(glyph->advance.x, 30 << 16);  // 600 units, 16.16
    FT_BBox box{};
    FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_UNSCALED, &box);
    EXPECT_EQ(std::tuple(box.xMin, box.yMin, box.xMax, box.yMax), std::tuple(0, 0, 30 * 64, 35 * 64));
    FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_PIXELS, &box);
    EXPECT_EQ(std::tuple(box.xMin, box.yMin, box.xMax, box.yMax), std::tuple(0, 0, 30, 35));
    FT_Glyph_Get_CBox(glyph, ft_glyph_bbox_gridfit, &box);
    EXPECT_EQ(box.xMax, 30 * 64);

    FT_Glyph copy{};
    ASSERT_EQ(FT_Glyph_Copy(glyph, &copy), FT_Err_Ok);
    FT_Matrix flip{-0x10000, 0, 0, 0x10000};
    FT_Vector shift{30 * 64, 0};
    ASSERT_EQ(FT_Glyph_Transform(copy, &flip, &shift), FT_Err_Ok);
    FT_Glyph_Get_CBox(copy, FT_GLYPH_BBOX_UNSCALED, &box);
    EXPECT_EQ(std::tuple(box.xMin, box.xMax), std::tuple(0, 30 * 64));  // mirrored in place

    // Stroked: the outline grows by the radius on every side.
    FT_Stroker stroker{};
    ASSERT_EQ(FT_Stroker_New(l.lib, &stroker), FT_Err_Ok);
    FT_Stroker_Set(stroker, 2 * 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
    FT_Glyph stroked = glyph;
    ASSERT_EQ(FT_Glyph_Copy(glyph, &stroked), FT_Err_Ok);
    ASSERT_EQ(FT_Glyph_Stroke(&stroked, stroker, 1), FT_Err_Ok);
    FT_Glyph_Get_CBox(stroked, FT_GLYPH_BBOX_UNSCALED, &box);
    EXPECT_LE(box.xMin, -2 * 64 + 8);
    EXPECT_GE(box.yMax, 37 * 64 - 8);
    FT_Glyph outer{};
    ASSERT_EQ(FT_Glyph_Copy(glyph, &outer), FT_Err_Ok);
    ASSERT_EQ(FT_Glyph_StrokeBorder(&outer, stroker, 0, 1), FT_Err_Ok);
    FT_Glyph_Get_CBox(outer, FT_GLYPH_BBOX_UNSCALED, &box);
    EXPECT_LE(box.xMin, -2 * 64 + 8);

    // To a bitmap glyph, replacing the outline glyph.
    FT_Vector origin{0, 0};
    ASSERT_EQ(FT_Glyph_To_Bitmap(&copy, FT_RENDER_MODE_NORMAL, &origin, 1), FT_Err_Ok);
    EXPECT_EQ(copy->format, FT_GLYPH_FORMAT_BITMAP);
    auto* bitmap = reinterpret_cast<FT_BitmapGlyph>(copy);
    EXPECT_EQ(bitmap->bitmap.width, 30u);
    EXPECT_EQ(bitmap->bitmap.rows, 35u);
    EXPECT_EQ(bitmap->top, 35);

    FT_Glyph empty{};
    ASSERT_EQ(FT_New_Glyph(l.lib, FT_GLYPH_FORMAT_OUTLINE, &empty), FT_Err_Ok);
    EXPECT_EQ(reinterpret_cast<FT_OutlineGlyph>(empty)->outline.n_points, 0);
    for (FT_Glyph g : {glyph, copy, stroked, outer, empty})
        FT_Done_Glyph(g);
    FT_Stroker_Done(stroker);
}

TEST(Stroker, PathsBordersAndExport) {
    Library l;
    FT_Stroker stroker{};
    ASSERT_EQ(FT_Stroker_New(l.lib, &stroker), FT_Err_Ok);
    FT_Stroker_Set(stroker, 32, FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_MITER_FIXED, 4 << 16);

    // An open path: a line, a conic and a cubic.
    FT_Vector p0{0, 0}, p1{640, 0}, c{960, 320}, p2{640, 640}, c1{320, 960}, c2{0, 960}, p3{0, 640};
    ASSERT_EQ(FT_Stroker_BeginSubPath(stroker, &p0, 1), FT_Err_Ok);
    ASSERT_EQ(FT_Stroker_LineTo(stroker, &p1), FT_Err_Ok);
    ASSERT_EQ(FT_Stroker_ConicTo(stroker, &c, &p2), FT_Err_Ok);
    ASSERT_EQ(FT_Stroker_CubicTo(stroker, &c1, &c2, &p3), FT_Err_Ok);
    ASSERT_EQ(FT_Stroker_EndSubPath(stroker), FT_Err_Ok);
    FT_UInt points = 0, contours = 0;
    ASSERT_EQ(FT_Stroker_GetCounts(stroker, &points, &contours), FT_Err_Ok);
    EXPECT_EQ(contours, 1u);  // an open path strokes into one closed contour
    FT_Outline outline{};
    ASSERT_EQ(FT_Outline_New(l.lib, points, contours, &outline), FT_Err_Ok);
    outline.n_points = outline.n_contours = 0;
    FT_Stroker_Export(stroker, &outline);
    EXPECT_EQ(static_cast<FT_UInt>(outline.n_points), points);
    EXPECT_EQ(FT_Outline_Check(&outline), FT_Err_Ok);
    FT_BBox box{};
    FT_Outline_Get_CBox(&outline, &box);
    EXPECT_EQ(box.yMin, -32);  // the bottom edge, half the width below
    FT_Outline_Done(l.lib, &outline);

    // A closed outline: both borders.
    FT_Stroker_Rewind(stroker);
    Square square(0, 0, 640);
    ASSERT_EQ(FT_Stroker_ParseOutline(stroker, &square.outline, 0), FT_Err_Ok);
    for (FT_StrokerBorder border : {FT_STROKER_BORDER_LEFT, FT_STROKER_BORDER_RIGHT}) {
        ASSERT_EQ(FT_Stroker_GetBorderCounts(stroker, border, &points, &contours), FT_Err_Ok);
        EXPECT_EQ(contours, 1u);
        ASSERT_EQ(FT_Outline_New(l.lib, points, contours, &outline), FT_Err_Ok);
        outline.n_points = outline.n_contours = 0;
        FT_Stroker_ExportBorder(stroker, border, &outline);
        FT_Outline_Get_CBox(&outline, &box);
        // Counter-clockwise: the left border is the inside one.
        if (border == FT_STROKER_BORDER_LEFT)
            EXPECT_EQ(std::tuple(box.xMin, box.xMax), std::tuple(32, 608));
        else
            EXPECT_EQ(std::tuple(box.xMin, box.xMax), std::tuple(-32, 672));
        FT_Outline_Done(l.lib, &outline);
    }
    FT_Stroker_Done(stroker);
}

TEST(Bitmap, ConvertCopyEmboldenBlend) {
    Library l;
    Face t(l.lib, "mwrap-test.ttf");
    ASSERT_EQ(FT_Set_Pixel_Sizes(t.face, 0, 20), FT_Err_Ok);
    ASSERT_EQ(FT_Load_Glyph(t.face, gid_A, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO | FT_LOAD_MONOCHROME), FT_Err_Ok);
    const FT_Bitmap& mono = t->glyph->bitmap;
    ASSERT_EQ(mono.pixel_mode, FT_PIXEL_MODE_MONO);

    FT_Bitmap gray;
    FT_Bitmap_Init(&gray);
    ASSERT_EQ(FT_Bitmap_Convert(l.lib, &mono, &gray, 4), FT_Err_Ok);
    EXPECT_EQ(gray.pixel_mode, FT_PIXEL_MODE_GRAY);
    EXPECT_EQ(gray.num_grays, 2);
    EXPECT_EQ(gray.width, mono.width);
    EXPECT_EQ(gray.pitch % 4, 0);
    std::uint64_t ones = 0;
    for (unsigned r = 0; r < mono.rows; ++r)
        for (unsigned x = 0; x < mono.width; ++x) {
            bool bit = mono.buffer[r * mono.pitch + x / 8] & (0x80 >> (x % 8));
            EXPECT_EQ(gray.buffer[r * gray.pitch + x], bit ? 1 : 0);
            ones += bit;
        }
    EXPECT_GT(ones, 0u);

    FT_Bitmap copy;
    FT_Bitmap_Init(&copy);
    ASSERT_EQ(FT_Bitmap_Copy(l.lib, &gray, &copy), FT_Err_Ok);
    EXPECT_EQ(coverage_sum(copy), coverage_sum(gray));
    ASSERT_EQ(FT_Bitmap_Embolden(l.lib, &copy, 64, 64), FT_Err_Ok);
    EXPECT_EQ(copy.width, gray.width + 1);
    EXPECT_EQ(copy.rows, gray.rows + 1);

    // Blend the coverage into a BGRA canvas in a colour.
    for (unsigned i = 0; i < gray.rows * static_cast<unsigned>(gray.pitch); ++i)
        gray.buffer[i] = gray.buffer[i] ? 255 : 0;
    gray.num_grays = 256;
    FT_Bitmap canvas;
    FT_Bitmap_Init(&canvas);
    FT_Vector at{0, 0};
    ASSERT_EQ(FT_Bitmap_Blend(l.lib, &gray, {0, 0}, &canvas, &at, FT_Color{0, 0, 255, 255}), FT_Err_Ok);
    EXPECT_EQ(canvas.pixel_mode, FT_PIXEL_MODE_BGRA);
    std::uint64_t red = 0;
    for (unsigned r = 0; r < canvas.rows; ++r)
        for (unsigned x = 0; x < canvas.width; ++x) {
            const unsigned char* p = canvas.buffer + r * canvas.pitch + 4 * x;
            if (p[3] == 255) {
                EXPECT_EQ(std::tuple(p[0], p[1], p[2]), std::tuple(0, 0, 255));
                ++red;
            }
        }
    EXPECT_EQ(red, ones);
    for (FT_Bitmap* b : {&gray, &copy, &canvas})
        EXPECT_EQ(FT_Bitmap_Done(l.lib, b), FT_Err_Ok);
}

TEST(Synthesis, EmboldenAndOblique) {
    Library l;
    Face t(l.lib, "mwrap-test.ttf");
    ASSERT_EQ(FT_Set_Pixel_Sizes(t.face, 0, 40), FT_Err_Ok);
    FT_BBox plain{}, box{};
    ASSERT_EQ(FT_Load_Glyph(t.face, gid_A, FT_LOAD_NO_HINTING), FT_Err_Ok);
    FT_Outline_Get_CBox(&t->glyph->outline, &plain);
    FT_Pos advance = t->glyph->advance.x;
    FT_GlyphSlot_Embolden(t->glyph);
    FT_Outline_Get_CBox(&t->glyph->outline, &box);
    EXPECT_GT(box.xMax - box.xMin, plain.xMax - plain.xMin);
    EXPECT_GT(t->glyph->advance.x, advance);

    // Oblique: the shear x += y × 0x0366A (about 12°), every point; Slant
    // takes both factors (y -= x × yslant).
    ASSERT_EQ(FT_Load_Glyph(t.face, gid_A, FT_LOAD_NO_HINTING), FT_Err_Ok);
    const std::vector<FT_Vector> points(t->glyph->outline.points, t->glyph->outline.points + t->glyph->outline.n_points);
    FT_GlyphSlot_Oblique(t->glyph);
    for (std::size_t i = 0; i < points.size(); ++i) {
        EXPECT_EQ(t->glyph->outline.points[i].x, points[i].x + FT_MulFix(points[i].y, 0x0366A)) << i;
        EXPECT_EQ(t->glyph->outline.points[i].y, points[i].y) << i;
    }
    ASSERT_EQ(FT_Load_Glyph(t.face, gid_A, FT_LOAD_NO_HINTING), FT_Err_Ok);
    FT_GlyphSlot_Slant(t->glyph, 0x1000, 0x2000);
    for (std::size_t i = 0; i < points.size(); ++i) {
        EXPECT_EQ(t->glyph->outline.points[i].x, points[i].x + FT_MulFix(points[i].y, 0x1000)) << i;
        EXPECT_EQ(t->glyph->outline.points[i].y, points[i].y + FT_MulFix(points[i].x, -0x2000)) << i;
    }
    ASSERT_EQ(FT_Load_Glyph(t.face, gid_A, FT_LOAD_NO_HINTING), FT_Err_Ok);
    FT_GlyphSlot_AdjustWeight(t->glyph, 0x0800, 0x0800);
    FT_Outline_Get_CBox(&t->glyph->outline, &box);
    EXPECT_GT(box.xMax - box.xMin, plain.xMax - plain.xMin);

    ASSERT_EQ(FT_Load_Glyph(t.face, gid_A, FT_LOAD_RENDER), FT_Err_Ok);
    EXPECT_EQ(FT_GlyphSlot_Own_Bitmap(t->glyph), FT_Err_Ok);
}

TEST(Color, PalettesLayersAndPaints) {
    Library l;
    Face t(l.lib, "mwrap-test.ttf");
    ASSERT_EQ(FT_Set_Pixel_Sizes(t.face, 0, 40), FT_Err_Ok);
    FT_Palette_Data palettes{};
    ASSERT_EQ(FT_Palette_Data_Get(t.face, &palettes), FT_Err_Ok);
    EXPECT_EQ(palettes.num_palettes, 2);
    EXPECT_EQ(palettes.num_palette_entries, 2);
    FT_Color* palette = nullptr;
    ASSERT_EQ(FT_Palette_Select(t.face, 1, &palette), FT_Err_Ok);
    ASSERT_NE(palette, nullptr);
    // CPAL stores BGRA: #1A33CC, then #CCE6FF at half alpha.
    EXPECT_EQ(std::tuple(palette[0].red, palette[0].green, palette[0].blue, palette[0].alpha),
              std::tuple(0x1A, 0x33, 0xCC, 0xFF));
    EXPECT_EQ(std::tuple(palette[1].red, palette[1].green, palette[1].blue, palette[1].alpha),
              std::tuple(0xCC, 0xE6, 0xFF, 0x80));
    EXPECT_EQ(FT_Palette_Set_Foreground_Color(t.face, FT_Color{0, 0, 0, 255}), FT_Err_Ok);

    // COLR v0: the heart's two layers, each a glyph and a palette entry.
    FT_LayerIterator layers{};
    FT_UInt glyph = 0, color = 0;
    std::vector<std::pair<FT_UInt, FT_UInt>> seen;
    while (FT_Get_Color_Glyph_Layer(t.face, gid_heart, &glyph, &color, &layers))
        seen.emplace_back(glyph, color);
    EXPECT_EQ(seen, (std::vector<std::pair<FT_UInt, FT_UInt>>{{gid_heart, 0}, {gid_heart_inner, 1}}));
    EXPECT_EQ(layers.num_layers, 2u);
    // Rendered in colour: blended layers in BGRA.
    ASSERT_EQ(FT_Load_Glyph(t.face, gid_heart, FT_LOAD_COLOR | FT_LOAD_RENDER), FT_Err_Ok);
    EXPECT_EQ(t->glyph->bitmap.pixel_mode, FT_PIXEL_MODE_BGRA);

    // COLR v1: the star is a layer list over a gradient and a solid fill.
    FT_OpaquePaint root{};
    ASSERT_TRUE(FT_Get_Color_Glyph_Paint(t.face, gid_star, FT_COLOR_NO_ROOT_TRANSFORM, &root));
    FT_COLR_Paint paint{};
    ASSERT_TRUE(FT_Get_Paint(t.face, root, &paint));
    ASSERT_EQ(paint.format, FT_COLR_PAINTFORMAT_COLR_LAYERS);
    FT_LayerIterator it = paint.u.colr_layers.layer_iterator;
    EXPECT_EQ(it.num_layers, 2u);
    std::vector<FT_COLR_Paint> glyph_paints;
    FT_OpaquePaint layer{};
    while (FT_Get_Paint_Layers(t.face, &it, &layer)) {
        FT_COLR_Paint p{};
        ASSERT_TRUE(FT_Get_Paint(t.face, layer, &p));
        glyph_paints.push_back(p);
    }
    ASSERT_EQ(glyph_paints.size(), 2u);
    EXPECT_EQ(glyph_paints[0].format, FT_COLR_PAINTFORMAT_GLYPH);
    EXPECT_EQ(glyph_paints[0].u.glyph.glyphID, gid_star);
    FT_COLR_Paint gradient{};
    ASSERT_TRUE(FT_Get_Paint(t.face, glyph_paints[0].u.glyph.paint, &gradient));
    ASSERT_EQ(gradient.format, FT_COLR_PAINTFORMAT_LINEAR_GRADIENT);
    EXPECT_EQ(gradient.u.linear_gradient.colorline.extend, FT_COLR_PAINT_EXTEND_PAD);
    EXPECT_EQ(gradient.u.linear_gradient.p1.x, 800 << 16);
    FT_ColorStopIterator stops = gradient.u.linear_gradient.colorline.color_stop_iterator;
    EXPECT_EQ(stops.num_color_stops, 2u);
    FT_ColorStop stop{};
    ASSERT_TRUE(FT_Get_Colorline_Stops(t.face, &stop, &stops));
    EXPECT_EQ(std::tuple(stop.stop_offset, stop.color.palette_index, stop.color.alpha), std::tuple(0, 0, 0x4000));
    ASSERT_TRUE(FT_Get_Colorline_Stops(t.face, &stop, &stops));
    EXPECT_EQ(std::tuple(stop.stop_offset, stop.color.palette_index), std::tuple(0x10000, 1));
    EXPECT_FALSE(FT_Get_Colorline_Stops(t.face, &stop, &stops));
    EXPECT_EQ(glyph_paints[1].u.glyph.glyphID, gid_heart_inner);
    FT_COLR_Paint solid{};
    ASSERT_TRUE(FT_Get_Paint(t.face, glyph_paints[1].u.glyph.paint, &solid));
    ASSERT_EQ(solid.format, FT_COLR_PAINTFORMAT_SOLID);
    EXPECT_EQ(std::tuple(solid.u.solid.color.palette_index, solid.u.solid.color.alpha), std::tuple(1, 0x2000));
    // The clip box, scaled to the size: 800 × 750 units at 40 px per 1000.
    FT_ClipBox clip{};
    ASSERT_TRUE(FT_Get_Color_Glyph_ClipBox(t.face, gid_star, &clip));
    EXPECT_EQ(std::tuple(clip.bottom_left.x, clip.bottom_left.y, clip.top_right.x, clip.top_right.y),
              std::tuple(0, 0, 32 * 64, 30 * 64));
    EXPECT_FALSE(FT_Get_Color_Glyph_Paint(t.face, gid_A, FT_COLOR_INCLUDE_ROOT_TRANSFORM, &root));
}

TEST(Svg, RendererHooks) {
    Library l;
    struct State {
        int init = 0, free = 0, preset = 0, render = 0;
        std::string document;
        FT_UShort start = 0, end = 0, units = 0;
    };
    static State state;
    state = {};
    SVG_RendererHooks hooks{};
    hooks.init_svg = [](FT_Pointer* data) -> FT_Error {
        *data = &state;
        return ++state.init, FT_Err_Ok;
    };
    hooks.free_svg = [](FT_Pointer*) { ++state.free; };
    hooks.preset_slot = [](FT_GlyphSlot slot, FT_Bool, FT_Pointer*) -> FT_Error {
        auto* doc = static_cast<FT_SVG_Document>(slot->other);
        state.document.assign(reinterpret_cast<const char*>(doc->svg_document), doc->svg_document_length);
        state.start = doc->start_glyph_id;
        state.end = doc->end_glyph_id;
        state.units = doc->units_per_EM;
        // A 30 × 30 BGRA image, as a rasteriser would size the 600-unit square.
        slot->bitmap.rows = slot->bitmap.width = 30;
        slot->bitmap.pitch = 30 * 4;
        slot->bitmap.pixel_mode = FT_PIXEL_MODE_BGRA;
        slot->bitmap_left = 5;
        slot->bitmap_top = 30;
        return ++state.preset, FT_Err_Ok;
    };
    hooks.render_svg = [](FT_GlyphSlot slot, FT_Pointer*) -> FT_Error {
        for (unsigned i = 0; i < slot->bitmap.rows * static_cast<unsigned>(slot->bitmap.pitch); i += 4) {
            unsigned char* p = slot->bitmap.buffer + i;
            p[0] = 0xC0, p[1] = 0x50, p[2] = 0x20, p[3] = 0xFF;  // #2050C0, BGRA
        }
        return ++state.render, FT_Err_Ok;
    };
    ASSERT_EQ(FT_Property_Set(l.lib, "ot-svg", "svg-hooks", &hooks), FT_Err_Ok);
    {
        Face t(l.lib, "mwrap-test.ttf");
        ASSERT_EQ(FT_Set_Pixel_Sizes(t.face, 0, 50), FT_Err_Ok);
        ASSERT_EQ(FT_Load_Glyph(t.face, gid_svgsquare, FT_LOAD_COLOR), FT_Err_Ok);
        EXPECT_EQ(t->glyph->format, FT_GLYPH_FORMAT_SVG);
        ASSERT_EQ(FT_Render_Glyph(t->glyph, FT_RENDER_MODE_NORMAL), FT_Err_Ok);
        EXPECT_EQ(t->glyph->bitmap.pixel_mode, FT_PIXEL_MODE_BGRA);  // the slot keeps format SVG
        EXPECT_EQ(t->glyph->bitmap.buffer[3], 0xFF);
        EXPECT_NE(state.document.find("<rect id=\"glyph10\""), std::string::npos);
        EXPECT_EQ(std::tuple(state.start, state.end, state.units), std::tuple(gid_svgsquare, gid_svgsquare, 1000));
        EXPECT_GE(state.preset, 1);
        EXPECT_EQ(state.render, 1);
        EXPECT_EQ(state.init, 1);
        // Without FT_LOAD_COLOR the glyph is its outline.
        ASSERT_EQ(FT_Load_Glyph(t.face, gid_svgsquare, FT_LOAD_DEFAULT), FT_Err_Ok);
        EXPECT_EQ(t->glyph->format, FT_GLYPH_FORMAT_OUTLINE);
    }
    FT_Done_FreeType(l.lib);
    l.lib = nullptr;
    EXPECT_EQ(state.free, 1);
}

TEST(Cache, ManagerCharmapsImagesAndSbits) {
    Library l;
    auto font = bytes("mwrap-test.ttf");
    struct Request {
        FT_Library lib;
        std::vector<FT_Byte>* font;
        int opened = 0;
    } request{l.lib, &font};
    auto requester = [](FTC_FaceID, FT_Library lib, FT_Pointer data, FT_Face* face) -> FT_Error {
        auto* r = static_cast<Request*>(data);
        ++r->opened;
        return FT_New_Memory_Face(lib, r->font->data(), static_cast<FT_Long>(r->font->size()), 0, face);
    };
    FTC_Manager manager{};
    ASSERT_EQ(FTC_Manager_New(l.lib, 0, 0, 0, requester, &request, &manager), FT_Err_Ok);
    auto id = reinterpret_cast<FTC_FaceID>(std::uintptr_t{1});
    FT_Face face{};
    ASSERT_EQ(FTC_Manager_LookupFace(manager, id, &face), FT_Err_Ok);
    EXPECT_STREQ(face->family_name, "mwrap test");
    ASSERT_EQ(FTC_Manager_LookupFace(manager, id, &face), FT_Err_Ok);
    EXPECT_EQ(request.opened, 1);  // cached

    FTC_ScalerRec scaler{id, 0, 24, 1, 0, 0};
    FT_Size size{};
    ASSERT_EQ(FTC_Manager_LookupSize(manager, &scaler, &size), FT_Err_Ok);
    EXPECT_EQ(size->metrics.y_ppem, 24);

    FTC_CMapCache cmaps{};
    ASSERT_EQ(FTC_CMapCache_New(manager, &cmaps), FT_Err_Ok);
    EXPECT_EQ(FTC_CMapCache_Lookup(cmaps, id, -1, 'A'), gid_A);
    EXPECT_EQ(FTC_CMapCache_Lookup(cmaps, id, -1, 'o'), gid_o);

    FTC_ImageCache images{};
    ASSERT_EQ(FTC_ImageCache_New(manager, &images), FT_Err_Ok);
    FTC_ImageTypeRec type{id, 24, 24, FT_LOAD_DEFAULT};
    FTC_ImageTypeRec same = type;
    EXPECT_TRUE(FTC_IMAGE_TYPE_COMPARE(&type, &same));
    FT_Glyph image{};
    FTC_Node node{};
    ASSERT_EQ(FTC_ImageCache_Lookup(images, &type, gid_A, &image, &node), FT_Err_Ok);
    EXPECT_EQ(image->format, FT_GLYPH_FORMAT_OUTLINE);
    FTC_Node_Unref(node, manager);
    ASSERT_EQ(FTC_ImageCache_LookupScaler(images, &scaler, FT_LOAD_RENDER, gid_A, &image, nullptr), FT_Err_Ok);
    EXPECT_EQ(image->format, FT_GLYPH_FORMAT_BITMAP);

    FTC_SBitCache sbits{};
    ASSERT_EQ(FTC_SBitCache_New(manager, &sbits), FT_Err_Ok);
    FTC_SBit sbit{};
    type.flags = FT_LOAD_RENDER;
    ASSERT_EQ(FTC_SBitCache_Lookup(sbits, &type, gid_A, &sbit, nullptr), FT_Err_Ok);
    EXPECT_NEAR(sbit->xadvance, 14, 1);  // 600 units at 24 px, hinted
    EXPECT_EQ(sbit->format, FT_PIXEL_MODE_GRAY);
    EXPECT_GT(sbit->width, 0);
    ASSERT_EQ(FTC_SBitCache_LookupScaler(sbits, &scaler, FT_LOAD_RENDER, gid_o, &sbit, nullptr), FT_Err_Ok);
    EXPECT_GT(sbit->height, 0);

    FTC_Manager_RemoveFaceID(manager, id);
    FTC_Manager_Reset(manager);
    ASSERT_EQ(FTC_Manager_LookupFace(manager, id, &face), FT_Err_Ok);
    EXPECT_EQ(request.opened, 2);
    FTC_Manager_Done(manager);
}

TEST(Arithmetic, FixedPointAndTrigonometry) {
    EXPECT_EQ(FT_MulFix(3 << 16, 0x8000), 3 << 15);
    EXPECT_EQ(FT_DivFix(1 << 16, 4 << 16), 0x4000);
    EXPECT_EQ(FT_MulDiv(1000, 3, 4), 750);
    EXPECT_EQ(FT_RoundFix(0x18000), 0x20000);
    EXPECT_EQ(FT_CeilFix(0x10001), 0x20000);
    EXPECT_EQ(FT_FloorFix(0x1FFFF), 0x10000);
    EXPECT_NEAR(FT_Cos(0), 0x10000, 1);
    EXPECT_NEAR(FT_Sin(FT_ANGLE_PI2), 0x10000, 1);
    EXPECT_NEAR(FT_Tan(FT_ANGLE_PI4), 0x10000, 2);
    EXPECT_NEAR(FT_Atan2(1 << 16, 1 << 16), FT_ANGLE_PI4, 1);
    EXPECT_EQ(FT_Angle_Diff(FT_ANGLE_PI2, -FT_ANGLE_PI2), FT_ANGLE_PI);  // normalised to (-π, π]
    EXPECT_EQ(FT_Angle_Diff(FT_ANGLE_PI4, FT_ANGLE_PI2), FT_ANGLE_PI4);
    FT_Vector v{};
    FT_Vector_Unit(&v, FT_ANGLE_PI2);
    EXPECT_NEAR(v.x, 0, 2);
    EXPECT_NEAR(v.y, 0x10000, 1);
    v = {3 << 16, 4 << 16};
    EXPECT_NEAR(FT_Vector_Length(&v), 5 << 16, 1);
    FT_Fixed length = 0;
    FT_Angle angle = 0;
    FT_Vector_Polarize(&v, &length, &angle);
    EXPECT_NEAR(length, 5 << 16, 1);
    EXPECT_NEAR(angle, FT_Atan2(3 << 16, 4 << 16), 1);
    FT_Vector back{};
    FT_Vector_From_Polar(&back, length, angle);
    EXPECT_NEAR(back.x, v.x, 4);
    EXPECT_NEAR(back.y, v.y, 4);
    FT_Vector_Rotate(&v, FT_ANGLE_PI);
    EXPECT_NEAR(v.x, -(3 << 16), 4);
    EXPECT_NEAR(v.y, -(4 << 16), 4);
    FT_Matrix scale{2 << 16, 0, 0, 3 << 16}, shear{1 << 16, 1 << 16, 0, 1 << 16};
    FT_Vector p{1 << 16, 1 << 16};
    FT_Vector_Transform(&p, &scale);
    EXPECT_EQ(std::tuple(p.x, p.y), std::tuple(2 << 16, 3 << 16));
    FT_Matrix_Multiply(&scale, &shear);  // shear := scale × shear
    EXPECT_EQ(std::tuple(shear.xx, shear.xy, shear.yx, shear.yy), std::tuple(2 << 16, 2 << 16, 0, 3 << 16));
    ASSERT_EQ(FT_Matrix_Invert(&scale), FT_Err_Ok);
    EXPECT_EQ(scale.xx, 0x8000);
    EXPECT_NEAR(scale.yy, 0x5555, 1);
    FT_Matrix singular{1 << 16, 1 << 16, 1 << 16, 1 << 16};
    EXPECT_EQ(FT_Matrix_Invert(&singular), FT_Err_Invalid_Argument);
}

TEST(List, AddFindIterateFinalize) {
    struct Memory {
        FT_MemoryRec_ rec{};
        int live = 0;
    } memory;
    memory.rec.user = &memory;
    memory.rec.alloc = [](FT_Memory m, long size) -> void* {
        ++static_cast<Memory*>(m->user)->live;
        return std::calloc(1, static_cast<std::size_t>(size));
    };
    memory.rec.free = [](FT_Memory m, void* block) {
        --static_cast<Memory*>(m->user)->live;
        std::free(block);
    };
    memory.rec.realloc = [](FT_Memory, long, long n, void* block) -> void* {
        return std::realloc(block, static_cast<std::size_t>(n));
    };
    FT_ListRec list{};
    EXPECT_TRUE(FT_IS_EMPTY(list));
    int values[3] = {10, 20, 30};
    for (int i = 0; i < 3; ++i) {
        auto node = static_cast<FT_ListNode>(memory.rec.alloc(&memory.rec, sizeof(FT_ListNodeRec)));
        node->data = &values[i];
        i == 2 ? FT_List_Insert(&list, node) : FT_List_Add(&list, node);  // 30, 10, 20
    }
    EXPECT_FALSE(FT_IS_EMPTY(list));
    FT_ListNode twenty = FT_List_Find(&list, &values[1]);
    ASSERT_NE(twenty, nullptr);
    FT_List_Up(&list, twenty);  // 20, 30, 10
    std::vector<int> order;
    EXPECT_EQ(FT_List_Iterate(
                  &list,
                  [](FT_ListNode node, void* user) -> FT_Error {
                      static_cast<std::vector<int>*>(user)->push_back(*static_cast<int*>(node->data));
                      return FT_Err_Ok;
                  },
                  &order),
              FT_Err_Ok);
    EXPECT_EQ(order, (std::vector<int>{20, 30, 10}));
    FT_ListNode thirty = FT_List_Find(&list, &values[2]);
    FT_List_Remove(&list, thirty);
    memory.rec.free(&memory.rec, thirty);
    int destroyed = 0;
    FT_List_Finalize(
        &list, [](FT_Memory, void*, void* user) { ++*static_cast<int*>(user); }, &memory.rec, &destroyed);
    EXPECT_EQ(destroyed, 2);
    EXPECT_TRUE(FT_IS_EMPTY(list));
    EXPECT_EQ(memory.live, 0);
}
