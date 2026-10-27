// Faces through import mcpplibs.mwrap.freetype: the library and its memory,
// faces from memory, files and compressed streams, sizes, character maps and
// variation sequences, names and SFNT tables, metrics, kerning, variable
// fonts, PostScript and BDF faces, modules and properties, errors.
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

// An FT_Memory over the C heap that counts its live blocks.
struct CountingMemory {
    FT_MemoryRec_ rec{};
    long live = 0;
    CountingMemory() {
        rec.user = this;
        rec.alloc = [](FT_Memory m, long size) -> void* {
            ++static_cast<CountingMemory*>(m->user)->live;
            return std::malloc(static_cast<std::size_t>(size));
        };
        rec.free = [](FT_Memory m, void* block) {
            --static_cast<CountingMemory*>(m->user)->live;
            std::free(block);
        };
        rec.realloc = [](FT_Memory, long, long new_size, void* block) -> void* {
            return std::realloc(block, static_cast<std::size_t>(new_size));
        };
    }
};

struct Library {
    FT_Library lib{};
    Library() { EXPECT_EQ(FT_Init_FreeType(&lib), FT_Err_Ok); }
    ~Library() { FT_Done_FreeType(lib); }
};

struct Face {
    std::vector<FT_Byte> buffer;
    FT_Face face{};
    Face(FT_Library lib, std::string_view name, FT_Long index = 0) : buffer(bytes(name)) {
        EXPECT_FALSE(buffer.empty()) << name;
        EXPECT_EQ(FT_New_Memory_Face(lib, buffer.data(), static_cast<FT_Long>(buffer.size()), index, &face), FT_Err_Ok)
            << name;
    }
    ~Face() {
        if (face)
            FT_Done_Face(face);
    }
    FT_Face operator->() const { return face; }
};

std::string sfnt_ascii(const FT_SfntName& name) {
    // Microsoft names are UTF-16BE; this keeps their ASCII.
    std::string out;
    for (FT_UInt i = 1; i < name.string_len; i += 2)
        out.push_back(static_cast<char>(name.string[i]));
    return out;
}

// The test font's glyph order (tools/make_test_fonts.py).
constexpr FT_UInt gid_A = 2, gid_V = 3, gid_o = 4, gid_emptyset = 8, gid_emptyset_slashed = 9;

}  // namespace

TEST(Library, VersionReferenceAndCustomMemory) {
    Library l;
    FT_Int major = 0, minor = 0, patch = 0;
    FT_Library_Version(l.lib, &major, &minor, &patch);
    EXPECT_EQ(major, FREETYPE_MAJOR);
    EXPECT_EQ(minor, FREETYPE_MINOR);
    EXPECT_EQ(patch, FREETYPE_PATCH);
    EXPECT_EQ(std::tuple(major, minor, patch), std::tuple(2, 13, 3));
    EXPECT_EQ(FT_Reference_Library(l.lib), FT_Err_Ok);
    EXPECT_EQ(FT_Done_Library(l.lib), FT_Err_Ok);  // one reference left
    EXPECT_EQ(FT_Get_TrueType_Engine_Type(l.lib), FT_TRUETYPE_ENGINE_TYPE_PATENTED);

    // A library on our own allocator: every block is returned.
    CountingMemory memory;
    FT_Library own{};
    ASSERT_EQ(FT_New_Library(&memory.rec, &own), FT_Err_Ok);
    FT_Add_Default_Modules(own);
    FT_Set_Default_Properties(own);
    {
        auto font = bytes("mwrap-test.ttf");
        FT_Face face{};
        ASSERT_EQ(FT_New_Memory_Face(own, font.data(), static_cast<FT_Long>(font.size()), 0, &face), FT_Err_Ok);
        EXPECT_EQ(FT_Set_Pixel_Sizes(face, 0, 24), FT_Err_Ok);
        EXPECT_EQ(FT_Load_Glyph(face, gid_A, FT_LOAD_RENDER), FT_Err_Ok);
        EXPECT_GT(memory.live, 0);
        FT_Done_Face(face);
    }
    EXPECT_EQ(FT_Done_Library(own), FT_Err_Ok);
    EXPECT_EQ(memory.live, 0);
}

TEST(Library, ModulesAndProperties) {
    Library l;
    EXPECT_NE(FT_Get_Module(l.lib, "truetype"), nullptr);
    EXPECT_NE(FT_Get_Module(l.lib, "cff"), nullptr);
    EXPECT_NE(FT_Get_Module(l.lib, "smooth"), nullptr);
    EXPECT_NE(FT_Get_Module(l.lib, "ot-svg"), nullptr);
    EXPECT_EQ(FT_Get_Module(l.lib, "no-such-module"), nullptr);
    EXPECT_EQ(FT_Add_Module(l.lib, nullptr), FT_Err_Invalid_Argument);

    FT_UInt version = TT_INTERPRETER_VERSION_35;
    EXPECT_EQ(FT_Property_Set(l.lib, "truetype", "interpreter-version", &version), FT_Err_Ok);
    FT_UInt got = 0;
    EXPECT_EQ(FT_Property_Get(l.lib, "truetype", "interpreter-version", &got), FT_Err_Ok);
    EXPECT_EQ(got, TT_INTERPRETER_VERSION_35);
    FT_UInt engine = FT_HINTING_ADOBE;
    EXPECT_EQ(FT_Property_Set(l.lib, "cff", "hinting-engine", &engine), FT_Err_Ok);
    EXPECT_EQ(FT_Property_Get(l.lib, "cff", "hinting-engine", &got), FT_Err_Ok);
    EXPECT_EQ(got, FT_HINTING_ADOBE);
    EXPECT_EQ(FT_Property_Set(l.lib, "truetype", "no-such-property", &version), FT_Err_Missing_Property);

    FT_Renderer smooth = FT_Get_Renderer(l.lib, FT_GLYPH_FORMAT_OUTLINE);
    ASSERT_NE(smooth, nullptr);
    EXPECT_EQ(FT_Set_Renderer(l.lib, smooth, 0, nullptr), FT_Err_Ok);

    // A driver removed is gone.
    EXPECT_EQ(FT_Remove_Module(l.lib, FT_Get_Module(l.lib, "bdf")), FT_Err_Ok);
    EXPECT_EQ(FT_Get_Module(l.lib, "bdf"), nullptr);
    EXPECT_EQ(FT_Remove_Module(l.lib, nullptr), FT_Err_Invalid_Driver_Handle);
}

TEST(Face, FromMemoryFileAndOpenArgs) {
    Library l;
    Face jb(l.lib, "JetBrainsMono.ttf");
    EXPECT_STREQ(jb->family_name, "JetBrains Mono");
    EXPECT_STREQ(jb->style_name, "Regular");
    EXPECT_EQ(jb->num_glyphs, 1179);
    EXPECT_EQ(jb->units_per_EM, 1000);
    EXPECT_TRUE(FT_IS_SCALABLE(jb.face));
    EXPECT_TRUE(FT_IS_SFNT(jb.face));
    EXPECT_TRUE(FT_HAS_HORIZONTAL(jb.face));
    EXPECT_TRUE(FT_IS_FIXED_WIDTH(jb.face));
    EXPECT_TRUE(FT_HAS_GLYPH_NAMES(jb.face));
    EXPECT_TRUE(FT_HAS_MULTIPLE_MASTERS(jb.face));
    EXPECT_FALSE(FT_HAS_FIXED_SIZES(jb.face));
    EXPECT_FALSE(FT_IS_NAMED_INSTANCE(jb.face));
    EXPECT_FALSE(FT_IS_TRICKY(jb.face));
    EXPECT_FALSE(FT_IS_CID_KEYED(jb.face));
    EXPECT_EQ(FT_HAS_FAST_GLYPHS(jb.face), 0);
    EXPECT_STREQ(FT_Get_Postscript_Name(jb.face), "JetBrainsMono-Regular");
    EXPECT_STREQ(FT_Get_Font_Format(jb.face), "TrueType");
    EXPECT_STREQ(FT_Get_X11_Font_Format(jb.face), "TrueType");
    EXPECT_STREQ(FT_FACE_DRIVER_NAME(jb.face), "truetype");
    EXPECT_EQ(jb->style_flags >> 16, 7);  // named instances: Thin … ExtraBold

    // The same font from its path, and through FT_Open_Args with parameters.
    FT_Face file{};
    ASSERT_EQ(FT_New_Face(l.lib, data("JetBrainsMono.ttf").string().c_str(), 0, &file), FT_Err_Ok);
    EXPECT_EQ(file->num_glyphs, jb->num_glyphs);
    EXPECT_NE(FT_Attach_File(file, data("no-such-file.afm").string().c_str()), FT_Err_Ok);
    EXPECT_EQ(FT_Reference_Face(file), FT_Err_Ok);
    EXPECT_EQ(FT_Done_Face(file), FT_Err_Ok);
    EXPECT_EQ(FT_Done_Face(file), FT_Err_Ok);

    auto font = bytes("mwrap-test.ttf");
    FT_Parameter params[] = {{FT_PARAM_TAG_IGNORE_SBIX, nullptr}};
    FT_Open_Args args{};
    args.flags = FT_OPEN_MEMORY | FT_OPEN_PARAMS;
    args.memory_base = font.data();
    args.memory_size = static_cast<FT_Long>(font.size());
    args.num_params = 1;
    args.params = params;
    FT_Face opened{};
    ASSERT_EQ(FT_Open_Face(l.lib, &args, 0, &opened), FT_Err_Ok);
    EXPECT_STREQ(opened->family_name, "mwrap test");
    // face_index -1: only count the faces.
    FT_Face probe{};
    ASSERT_EQ(FT_Open_Face(l.lib, &args, -1, &probe), FT_Err_Ok);
    EXPECT_EQ(probe->num_faces, 1);
    FT_Done_Face(probe);
    FT_Done_Face(opened);
}

TEST(Face, CompressedStreams) {
    Library l;
    CountingMemory memory;
    auto original = bytes("mwrap-test.ttf");
    for (auto [name, gzip] : {std::pair{"mwrap-test.ttf.gz", true}, std::pair{"mwrap-test.ttf.Z", false}}) {
        auto packed = bytes(name);
        FT_StreamRec source{};  // a memory stream: base and size, no read function
        source.base = packed.data();
        source.size = packed.size();
        source.memory = &memory.rec;
        auto open = [&](FT_StreamRec& stream) {
            return gzip ? FT_Stream_OpenGzip(&stream, &source) : FT_Stream_OpenLZW(&stream, &source);
        };
        FT_StreamRec stream{};
        ASSERT_EQ(open(stream), FT_Err_Ok) << name;
        // A small gzip stream is decompressed whole into memory (base, no read
        // function), which FT_Stream_Read handles; LZW is read on demand.
        EXPECT_EQ(stream.read == nullptr, gzip) << name;
        std::vector<unsigned char> out(original.size() + 16);
        unsigned long got = stream.read ? stream.read(&stream, 0, out.data(), out.size())
                                        : (std::copy_n(stream.base, stream.size, out.begin()), stream.size);
        EXPECT_EQ(got, original.size()) << name;
        EXPECT_TRUE(std::equal(original.begin(), original.end(), out.begin())) << name;
        stream.close(&stream);
        EXPECT_EQ(memory.live, 0) << name;  // every block back

        // A face reads through the decompressing stream. FT_Open_Face gives the
        // stream the library's allocator, and FT_Done_Face closes it with that.
        FT_StreamRec face_stream{};
        ASSERT_EQ(open(face_stream), FT_Err_Ok) << name;
        FT_Open_Args args{};
        args.flags = FT_OPEN_STREAM;
        args.stream = &face_stream;
        FT_Face face{};
        ASSERT_EQ(FT_Open_Face(l.lib, &args, 0, &face), FT_Err_Ok) << name;
        EXPECT_STREQ(face->family_name, "mwrap test");
        EXPECT_EQ(FT_Load_Glyph(face, 2, FT_LOAD_NO_SCALE), FT_Err_Ok);
        EXPECT_EQ(face->glyph->metrics.horiAdvance, 600);
        FT_Done_Face(face);
        memory.live = 0;  // the blocks the library's allocator released
    }

    // Whole-buffer gzip, and the configuration's absent bzip2.
    auto packed = bytes("mwrap-test.ttf.gz");
    std::vector<FT_Byte> out(original.size());
    FT_ULong length = out.size();
    EXPECT_EQ(FT_Gzip_Uncompress(&memory.rec, out.data(), &length, packed.data(), packed.size()), FT_Err_Ok);
    EXPECT_EQ(length, original.size());
    EXPECT_EQ(out, original);
    FT_StreamRec source{}, stream{};
    source.base = packed.data();
    source.size = packed.size();
    source.memory = &memory.rec;
    EXPECT_EQ(FT_Stream_OpenBzip2(&stream, &source), FT_Err_Unimplemented_Feature);
    EXPECT_EQ(FT_Stream_OpenLZW(&stream, &source), FT_Err_Invalid_File_Format);
}

TEST(Face, SizesAndScaling) {
    Library l;
    Face jb(l.lib, "JetBrainsMono.ttf");
    EXPECT_EQ(FT_Set_Char_Size(jb.face, 0, 16 * 64, 300, 300), FT_Err_Ok);
    EXPECT_EQ(jb->size->metrics.y_ppem, 67);  // 16 pt at 300 dpi
    EXPECT_EQ(FT_Set_Pixel_Sizes(jb.face, 0, 20), FT_Err_Ok);
    EXPECT_EQ(jb->size->metrics.y_ppem, 20);
    EXPECT_EQ(jb->size->metrics.x_ppem, 20);
    FT_Size_RequestRec request{FT_SIZE_REQUEST_TYPE_NOMINAL, 0, 32 * 64, 72, 72};
    EXPECT_EQ(FT_Request_Size(jb.face, &request), FT_Err_Ok);
    EXPECT_EQ(jb->size->metrics.y_ppem, 32);
    EXPECT_EQ(FT_MulFix(1000, jb->size->metrics.y_scale), 32 * 64);  // units_per_EM → 32 px in 26.6

    // A second size object, activated and released.
    FT_Size first = jb->size, second{};
    ASSERT_EQ(FT_New_Size(jb.face, &second), FT_Err_Ok);
    EXPECT_EQ(FT_Activate_Size(second), FT_Err_Ok);
    EXPECT_EQ(FT_Set_Pixel_Sizes(jb.face, 0, 12), FT_Err_Ok);
    EXPECT_EQ(second->metrics.y_ppem, 12);
    EXPECT_EQ(first->metrics.y_ppem, 32);
    EXPECT_EQ(FT_Activate_Size(first), FT_Err_Ok);
    EXPECT_EQ(FT_Done_Size(second), FT_Err_Ok);
    EXPECT_EQ(jb->size->metrics.y_ppem, 32);

    // A transform applies to what is loaded next, and is read back.
    FT_Matrix matrix{0x10000, 0x5000, 0, 0x10000};
    FT_Vector delta{64, 0};
    FT_Set_Transform(jb.face, &matrix, &delta);
    FT_Matrix m{};
    FT_Vector d{};
    FT_Get_Transform(jb.face, &m, &d);
    EXPECT_EQ(m.xy, 0x5000);
    EXPECT_EQ(d.x, 64);
    FT_Set_Transform(jb.face, nullptr, nullptr);
}

TEST(Face, CharacterMapsAndVariationSequences) {
    Library l;
    Face t(l.lib, "mwrap-test.ttf");
    EXPECT_EQ(FT_Select_Charmap(t.face, FT_ENCODING_UNICODE), FT_Err_Ok);
    EXPECT_EQ(t->charmap->encoding, FT_ENCODING_UNICODE);
    EXPECT_EQ(t->charmap->encoding, ft_encoding_unicode);
    EXPECT_GE(FT_Get_Charmap_Index(t->charmap), 0);
    EXPECT_EQ(FT_Get_CMap_Format(t->charmap), 4);
    EXPECT_EQ(FT_Get_CMap_Language_ID(t->charmap), 0u);
    EXPECT_EQ(FT_Set_Charmap(t.face, t->charmaps[0]), FT_Err_Ok);
    EXPECT_EQ(FT_Select_Charmap(t.face, FT_ENCODING_UNICODE), FT_Err_Ok);
    EXPECT_EQ(FT_Select_Charmap(t.face, FT_ENCODING_BIG5), FT_Err_Invalid_Argument);

    EXPECT_EQ(FT_Get_Char_Index(t.face, 'A'), gid_A);
    EXPECT_EQ(FT_Get_Char_Index(t.face, 'Z'), 0u);
    std::vector<FT_ULong> codes;
    FT_UInt gindex = 0;
    for (FT_ULong c = FT_Get_First_Char(t.face, &gindex); gindex != 0; c = FT_Get_Next_Char(t.face, c, &gindex))
        codes.push_back(c);
    EXPECT_EQ(codes, (std::vector<FT_ULong>{0x20, 0x41, 0x56, 0x6F, 0x2205, 0x25A0, 0x2605, 0x2665}));

    char name[32]{};
    EXPECT_EQ(FT_Get_Glyph_Name(t.face, gid_o, name, sizeof name), FT_Err_Ok);
    EXPECT_STREQ(name, "o");
    EXPECT_EQ(FT_Get_Name_Index(t.face, "emptyset.slashed"), gid_emptyset_slashed);

    // Unicode variation sequences (cmap format 14).
    EXPECT_EQ(FT_Face_GetCharVariantIndex(t.face, 0x2205, 0xFE00), gid_emptyset_slashed);
    EXPECT_EQ(FT_Face_GetCharVariantIsDefault(t.face, 0x2205, 0xFE00), 0);
    EXPECT_EQ(FT_Get_Char_Index(t.face, 0x2205), gid_emptyset);
    FT_UInt32* selectors = FT_Face_GetVariantSelectors(t.face);
    ASSERT_NE(selectors, nullptr);
    EXPECT_EQ(selectors[0], 0xFE00u);
    EXPECT_EQ(selectors[1], 0u);
    FT_UInt32* variants = FT_Face_GetVariantsOfChar(t.face, 0x2205);
    ASSERT_NE(variants, nullptr);
    EXPECT_EQ(variants[0], 0xFE00u);
    FT_UInt32* chars = FT_Face_GetCharsOfVariant(t.face, 0xFE00);
    ASSERT_NE(chars, nullptr);
    EXPECT_EQ(chars[0], 0x2205u);
    EXPECT_EQ(chars[1], 0u);
}

TEST(Face, NamesTablesAndFlags) {
    Library l;
    Face t(l.lib, "mwrap-test.ttf");
    FT_UInt count = FT_Get_Sfnt_Name_Count(t.face);
    EXPECT_GT(count, 0u);
    std::map<FT_UShort, std::string> names;
    for (FT_UInt i = 0; i < count; ++i) {
        FT_SfntName n{};
        ASSERT_EQ(FT_Get_Sfnt_Name(t.face, i, &n), FT_Err_Ok);
        if (n.platform_id == TT_PLATFORM_MICROSOFT && n.encoding_id == TT_MS_ID_UNICODE_CS &&
            n.language_id == TT_MS_LANGID_ENGLISH_UNITED_STATES)
            names[n.name_id] = sfnt_ascii(n);
    }
    EXPECT_EQ(names[TT_NAME_ID_FONT_FAMILY], "mwrap test");
    EXPECT_EQ(names[TT_NAME_ID_FONT_SUBFAMILY], "Regular");
    EXPECT_EQ(names[TT_NAME_ID_PS_NAME], "mwraptest-Regular");
    FT_SfntLangTag tag{};
    EXPECT_EQ(FT_Get_Sfnt_LangTag(t.face, 0x8000, &tag), FT_Err_Invalid_Table);  // name table format 0: no tags

    auto* head = static_cast<TT_Header*>(FT_Get_Sfnt_Table(t.face, FT_SFNT_HEAD));
    ASSERT_NE(head, nullptr);
    EXPECT_EQ(head->Units_Per_EM, 1000);
    auto* os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(t.face, FT_SFNT_OS2));
    ASSERT_NE(os2, nullptr);
    EXPECT_EQ(os2->usWeightClass, 400);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(os2->achVendID), 4), "MCPP");
    auto* hhea = static_cast<TT_HoriHeader*>(FT_Get_Sfnt_Table(t.face, FT_SFNT_HHEA));
    ASSERT_NE(hhea, nullptr);
    EXPECT_EQ(hhea->Ascender, 800);
    EXPECT_NE(FT_Get_Sfnt_Table(t.face, FT_SFNT_VHEA), nullptr);
    EXPECT_NE(FT_Get_Sfnt_Table(t.face, FT_SFNT_POST), nullptr);
    auto* maxp = static_cast<TT_MaxProfile*>(FT_Get_Sfnt_Table(t.face, FT_SFNT_MAXP));
    ASSERT_NE(maxp, nullptr);
    EXPECT_EQ(maxp->numGlyphs, 11);
    EXPECT_EQ(FT_Get_Sfnt_Table(t.face, FT_SFNT_PCLT), nullptr);

    FT_ULong length = 0;
    ASSERT_EQ(FT_Load_Sfnt_Table(t.face, TTAG_head, 0, nullptr, &length), FT_Err_Ok);
    EXPECT_EQ(length, 54u);
    std::vector<FT_Byte> raw(length);
    ASSERT_EQ(FT_Load_Sfnt_Table(t.face, TTAG_head, 0, raw.data(), &length), FT_Err_Ok);
    EXPECT_EQ(raw[0] << 24 | raw[1] << 16 | raw[2] << 8 | raw[3], 0x00010000);
    EXPECT_EQ(FT_Load_Sfnt_Table(t.face, TTAG_GSUB, 0, nullptr, &length), FT_Err_Table_Missing);
    FT_ULong tables = 0;
    ASSERT_EQ(FT_Sfnt_Table_Info(t.face, 0, nullptr, &tables), FT_Err_Ok);
    std::set<FT_ULong> tags;
    for (FT_UInt i = 0; i < tables; ++i) {
        FT_ULong tg = 0, len = 0;
        ASSERT_EQ(FT_Sfnt_Table_Info(t.face, i, &tg, &len), FT_Err_Ok);
        tags.insert(tg);
    }
    for (FT_ULong expected : {TTAG_COLR, TTAG_CPAL, TTAG_SVG, TTAG_kern, TTAG_glyf, TTAG_cmap, FT_MAKE_TAG('v', 'm', 't', 'x')})
        EXPECT_TRUE(tags.contains(expected)) << expected;

    EXPECT_EQ(FT_Get_FSType_Flags(t.face), FT_FSTYPE_EDITABLE_EMBEDDING);
    EXPECT_FALSE(FT_Face_CheckTrueTypePatents(t.face));
    EXPECT_FALSE(FT_Face_SetUnpatentedHinting(t.face, 1));
    EXPECT_TRUE(FT_HAS_VERTICAL(t.face));
    EXPECT_TRUE(FT_HAS_KERNING(t.face));
    EXPECT_TRUE(FT_HAS_COLOR(t.face));
    EXPECT_TRUE(FT_HAS_SVG(t.face));
    EXPECT_FALSE(FT_HAS_SBIX(t.face));
    EXPECT_FALSE(FT_HAS_SBIX_OVERLAY(t.face));
    EXPECT_FALSE(FT_HAS_MULTIPLE_MASTERS(t.face));
    EXPECT_FALSE(FT_IS_VARIATION(t.face));

    Face jb(l.lib, "JetBrainsMono.ttf");
    EXPECT_EQ(FT_Get_Gasp(jb.face, 12),
              FT_GASP_DO_GRIDFIT | FT_GASP_DO_GRAY | FT_GASP_SYMMETRIC_GRIDFIT | FT_GASP_SYMMETRIC_SMOOTHING);
    EXPECT_EQ(FT_Get_Gasp(t.face, 12), FT_GASP_NO_TABLE);
    EXPECT_EQ(FT_Get_FSType_Flags(jb.face), FT_FSTYPE_INSTALLABLE_EMBEDDING);
}

TEST(Face, AdvancesAndKerning) {
    Library l;
    Face t(l.lib, "mwrap-test.ttf");
    FT_Fixed advance = 0;
    EXPECT_EQ(FT_Get_Advance(t.face, gid_A, FT_LOAD_NO_SCALE, &advance), FT_Err_Ok);
    EXPECT_EQ(advance, 600);
    FT_Fixed advances[3]{};
    EXPECT_EQ(FT_Get_Advances(t.face, gid_A, 3, FT_LOAD_NO_SCALE, advances), FT_Err_Ok);
    EXPECT_EQ(advances[0], 600);
    EXPECT_EQ(advances[2], 600);
    EXPECT_EQ(FT_Get_Advance(t.face, gid_A, FT_LOAD_NO_SCALE | FT_LOAD_VERTICAL_LAYOUT, &advance), FT_Err_Ok);
    EXPECT_EQ(advance, 1000);

    FT_Vector kern{};
    EXPECT_EQ(FT_Get_Kerning(t.face, gid_A, gid_V, FT_KERNING_UNSCALED, &kern), FT_Err_Ok);
    EXPECT_EQ(kern.x, -80);
    EXPECT_EQ(FT_Get_Kerning(t.face, gid_V, gid_o, FT_KERNING_UNSCALED, &kern), FT_Err_Ok);
    EXPECT_EQ(kern.x, -40);
    EXPECT_EQ(FT_Get_Kerning(t.face, gid_o, gid_A, FT_KERNING_UNSCALED, &kern), FT_Err_Ok);
    EXPECT_EQ(kern.x, 0);
    ASSERT_EQ(FT_Set_Pixel_Sizes(t.face, 0, 100), FT_Err_Ok);  // 1 unit = 0.1 px
    EXPECT_EQ(FT_Get_Kerning(t.face, gid_A, gid_V, FT_KERNING_UNFITTED, &kern), FT_Err_Ok);
    EXPECT_EQ(kern.x, -8 * 64);
    EXPECT_EQ(FT_Get_Kerning(t.face, gid_A, gid_V, FT_KERNING_DEFAULT, &kern), FT_Err_Ok);
    EXPECT_EQ(kern.x, -8 * 64);
    EXPECT_EQ(FT_Get_Kerning(t.face, gid_A, gid_V, ft_kerning_default, &kern), FT_Err_Ok);
    FT_Fixed track = 0;
    EXPECT_EQ(FT_Get_Track_Kerning(t.face, 12 << 16, 0, &track), FT_Err_Unimplemented_Feature);  // Type 1 only

    // PFR queries on another format: the face's own data, and an error that
    // says it is not PFR.
    FT_UInt outline_resolution = 0, metrics_resolution = 0;
    FT_Fixed x_scale = 0, y_scale = 0;
    EXPECT_EQ(FT_Get_PFR_Metrics(t.face, &outline_resolution, &metrics_resolution, &x_scale, &y_scale),
              FT_Err_Unknown_File_Format);
    EXPECT_EQ(outline_resolution, 1000u);
    EXPECT_EQ(x_scale, t->size->metrics.x_scale);
    EXPECT_EQ(FT_Get_PFR_Kerning(t.face, gid_A, gid_V, &kern), FT_Err_Ok);
    EXPECT_EQ(kern.x, -80);
    FT_Pos pfr_advance = 0;
    EXPECT_EQ(FT_Get_PFR_Advance(t.face, gid_A, &pfr_advance), FT_Err_Invalid_Argument);
    FT_WinFNT_HeaderRec header{};
    EXPECT_EQ(FT_Get_WinFNT_Header(t.face, &header), FT_Err_Invalid_Argument);
}

TEST(Face, VariableFont) {
    Library l;
    Face jb(l.lib, "JetBrainsMono.ttf");
    FT_MM_Var* mm = nullptr;
    ASSERT_EQ(FT_Get_MM_Var(jb.face, &mm), FT_Err_Ok);
    ASSERT_EQ(mm->num_axis, 1u);
    EXPECT_EQ(mm->axis[0].tag, FT_MAKE_TAG('w', 'g', 'h', 't'));
    EXPECT_EQ(mm->axis[0].minimum, 100 << 16);
    EXPECT_EQ(mm->axis[0].def, 400 << 16);
    EXPECT_EQ(mm->axis[0].maximum, 800 << 16);
    EXPECT_EQ(mm->num_namedstyles, 7u);
    FT_UInt flags = 99;
    EXPECT_EQ(FT_Get_Var_Axis_Flags(mm, 0, &flags), FT_Err_Ok);
    EXPECT_EQ(flags & FT_VAR_AXIS_FLAG_HIDDEN, 0u);
    FT_UInt default_instance = 0;
    EXPECT_EQ(FT_Get_Default_Named_Instance(jb.face, &default_instance), FT_Err_Ok);
    EXPECT_EQ(default_instance, 4u);  // Regular, 1-based

    FT_Load_Glyph(jb.face, FT_Get_Char_Index(jb.face, 'o'), FT_LOAD_NO_SCALE);
    FT_BBox regular{};
    FT_Outline_Get_CBox(&jb->glyph->outline, &regular);

    FT_Fixed bold[] = {700 << 16};
    ASSERT_EQ(FT_Set_Var_Design_Coordinates(jb.face, 1, bold), FT_Err_Ok);
    EXPECT_TRUE(FT_IS_VARIATION(jb.face));
    FT_Fixed coords[1]{};
    EXPECT_EQ(FT_Get_Var_Design_Coordinates(jb.face, 1, coords), FT_Err_Ok);
    EXPECT_EQ(coords[0], 700 << 16);
    EXPECT_EQ(FT_Get_Var_Blend_Coordinates(jb.face, 1, coords), FT_Err_Ok);
    EXPECT_GT(coords[0], 0);
    EXPECT_LE(coords[0], 0x10000);
    EXPECT_EQ(FT_Get_MM_Blend_Coordinates(jb.face, 1, coords), FT_Err_Ok);
    FT_Load_Glyph(jb.face, FT_Get_Char_Index(jb.face, 'o'), FT_LOAD_NO_SCALE);
    FT_BBox heavy{};
    FT_Outline_Get_CBox(&jb->glyph->outline, &heavy);
    EXPECT_NE(heavy.xMin, regular.xMin);  // the stems thicken

    FT_Fixed normalized[] = {0};
    EXPECT_EQ(FT_Set_Var_Blend_Coordinates(jb.face, 1, normalized), FT_Err_Ok);
    EXPECT_EQ(FT_Set_MM_Blend_Coordinates(jb.face, 1, normalized), FT_Err_Ok);
    EXPECT_EQ(FT_Set_Named_Instance(jb.face, 6), FT_Err_Ok);  // Bold
    EXPECT_EQ(FT_Get_Var_Design_Coordinates(jb.face, 1, coords), FT_Err_Ok);
    EXPECT_EQ(coords[0], 700 << 16);
    EXPECT_TRUE(FT_IS_NAMED_INSTANCE(jb.face));
    // Type 1 multiple-master queries refuse a TrueType GX font.
    FT_Multi_Master t1mm{};
    EXPECT_NE(FT_Get_Multi_Master(jb.face, &t1mm), FT_Err_Ok);
    FT_Long design[] = {500};
    EXPECT_NE(FT_Set_MM_Design_Coordinates(jb.face, 1, design), FT_Err_Ok);
    FT_UInt weights_length = 1;
    FT_Fixed weights[1]{};
    EXPECT_NE(FT_Get_MM_WeightVector(jb.face, &weights_length, weights), FT_Err_Ok);
    EXPECT_NE(FT_Set_MM_WeightVector(jb.face, 1, weights), FT_Err_Ok);
    EXPECT_EQ(FT_Done_MM_Var(l.lib, mm), FT_Err_Ok);

    // A named instance opened directly: face_index carries it in bits 16-30.
    Face bold_face(l.lib, "JetBrainsMono.ttf", (6 << 16) | 0);
    EXPECT_TRUE(FT_IS_NAMED_INSTANCE(bold_face.face));
    EXPECT_STREQ(bold_face->style_name, "Bold");
}

TEST(Face, PostScriptAndCID) {
    Library l;
    Face cff(l.lib, "mwrap-test.otf");
    EXPECT_STREQ(FT_Get_Font_Format(cff.face), "CFF");
    EXPECT_STREQ(FT_FACE_DRIVER_NAME(cff.face), "cff");
    PS_FontInfoRec info{};
    ASSERT_EQ(FT_Get_PS_Font_Info(cff.face, &info), FT_Err_Ok);
    EXPECT_STREQ(info.full_name, "mwrap test CFF Regular");
    EXPECT_STREQ(info.family_name, "mwrap test CFF");
    EXPECT_STREQ(info.weight, "Regular");
    EXPECT_TRUE(FT_Has_PS_Glyph_Names(cff.face));
    PS_PrivateRec priv{};
    EXPECT_NE(FT_Get_PS_Font_Private(cff.face, &priv), FT_Err_Ok);  // CFF keeps no Type 1 private dict
    EXPECT_EQ(FT_Get_PS_Font_Value(cff.face, PS_DICT_FONT_NAME, 0, nullptr, 0), 0);  // Type 1 dictionaries only
    FT_Bool cid_keyed = 1;
    EXPECT_EQ(FT_Get_CID_Is_Internally_CID_Keyed(cff.face, &cid_keyed), FT_Err_Ok);
    EXPECT_FALSE(cid_keyed);
    const char *registry = nullptr, *ordering = nullptr;
    FT_Int supplement = 0;
    EXPECT_NE(FT_Get_CID_Registry_Ordering_Supplement(cff.face, &registry, &ordering, &supplement), FT_Err_Ok);
    FT_UInt cid = 0;
    EXPECT_NE(FT_Get_CID_From_Glyph_Index(cff.face, gid_A, &cid), FT_Err_Ok);

    Face t(l.lib, "mwrap-test.ttf");
    EXPECT_EQ(FT_Get_PS_Font_Info(t.face, &info), FT_Err_Invalid_Argument);
    EXPECT_FALSE(FT_Has_PS_Glyph_Names(t.face));  // claimed only by PostScript-based formats
    EXPECT_TRUE(FT_HAS_GLYPH_NAMES(t.face));        // the post table's names
}

TEST(Face, BdfBitmapFont) {
    Library l;
    const std::string bdf = R"(STARTFONT 2.1
FONT -mcpp-mwrap-Medium-R-Normal--8-80-75-75-C-80-ISO10646-1
SIZE 8 75 75
FONTBOUNDINGBOX 8 8 0 -1
STARTPROPERTIES 4
FONT_ASCENT 7
FONT_DESCENT 1
CHARSET_REGISTRY "ISO10646"
CHARSET_ENCODING "1"
ENDPROPERTIES
CHARS 1
STARTCHAR A
ENCODING 65
SWIDTH 500 0
DWIDTH 8 0
BBX 8 8 0 -1
BITMAP
18
24
42
42
7E
42
42
00
ENDCHAR
ENDFONT
)";
    FT_Face face{};
    ASSERT_EQ(FT_New_Memory_Face(l.lib, reinterpret_cast<const FT_Byte*>(bdf.data()), static_cast<FT_Long>(bdf.size()), 0,
                                 &face),
              FT_Err_Ok);
    EXPECT_STREQ(FT_Get_Font_Format(face), "BDF");
    EXPECT_TRUE(FT_HAS_FIXED_SIZES(face));
    EXPECT_FALSE(FT_IS_SCALABLE(face));
    ASSERT_EQ(face->num_fixed_sizes, 1);
    EXPECT_EQ(face->available_sizes[0].height, 8);
    const char *encoding = nullptr, *registry = nullptr;
    ASSERT_EQ(FT_Get_BDF_Charset_ID(face, &encoding, &registry), FT_Err_Ok);
    EXPECT_STREQ(registry, "ISO10646");
    EXPECT_STREQ(encoding, "1");
    BDF_PropertyRec property{};
    ASSERT_EQ(FT_Get_BDF_Property(face, "FONT_ASCENT", &property), FT_Err_Ok);
    EXPECT_EQ(property.type, BDF_PROPERTY_TYPE_INTEGER);
    EXPECT_EQ(property.u.integer, 7);
    EXPECT_EQ(FT_Select_Size(face, 0), FT_Err_Ok);
    ASSERT_EQ(FT_Load_Char(face, 'A', FT_LOAD_RENDER), FT_Err_Ok);
    const FT_Bitmap& bitmap = face->glyph->bitmap;
    EXPECT_EQ(bitmap.pixel_mode, FT_PIXEL_MODE_MONO);
    EXPECT_EQ(bitmap.pixel_mode, ft_pixel_mode_mono);
    EXPECT_EQ(bitmap.width, 8u);
    EXPECT_EQ(bitmap.rows, 8u);
    const unsigned char rows[] = {0x18, 0x24, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x00};
    for (unsigned r = 0; r < 8; ++r)
        EXPECT_EQ(bitmap.buffer[r * bitmap.pitch], rows[r]) << r;
    EXPECT_EQ(face->glyph->advance.x, 8 * 64);
    FT_Done_Face(face);
}

TEST(Errors, CodesAndStrings) {
    Library l;
    const std::vector<FT_Byte> garbage(4096, 0x5A);  // no format's signature
    FT_Face face{};
    FT_Error error = FT_New_Memory_Face(l.lib, garbage.data(), static_cast<FT_Long>(garbage.size()), 0, &face);
    EXPECT_EQ(error, FT_Err_Unknown_File_Format);
    EXPECT_EQ(FT_ERROR_BASE(error), FT_Err_Unknown_File_Format);
    EXPECT_EQ(FT_ERROR_MODULE(error), 0u);
    EXPECT_EQ(FT_ERROR_BASE(FT_Err_Out_Of_Memory | FT_Mod_Err_Base), FT_Err_Out_Of_Memory);
    EXPECT_EQ(FT_Err_Ok, 0);
    EXPECT_EQ(FT_Err_Cannot_Open_Resource, 0x01);
    EXPECT_EQ(FT_Err_Invalid_Glyph_Index, 0x10);
    EXPECT_EQ(FT_Err_Missing_SVG_Hooks, 0x9E);
    EXPECT_GT(FT_Err_Max, FT_Err_Missing_SVG_Hooks);
    EXPECT_EQ(FT_Mod_Err_Base, 0);
    // FT_CONFIG_OPTION_ERROR_STRINGS is off in this configuration, as in
    // upstream's default ftoption.h: FT_Error_String has no strings to return.
    EXPECT_EQ(FT_Error_String(FT_Err_Unknown_File_Format), nullptr);
    EXPECT_EQ(FT_New_Face(l.lib, data("no-such-font.ttf").string().c_str(), 0, &face), FT_Err_Cannot_Open_Resource);
    EXPECT_EQ(FT_Set_Char_Size(nullptr, 0, 64, 72, 72), FT_Err_Invalid_Face_Handle);
    // The validation modules are not built (upstream's default modules.cfg).
    Face t(l.lib, "mwrap-test.ttf");
    FT_Bytes base{}, gdef{}, gpos{}, gsub{}, jstf{};
    EXPECT_EQ(FT_OpenType_Validate(t.face, FT_VALIDATE_OT, &base, &gdef, &gpos, &gsub, &jstf),
              FT_Err_Unimplemented_Feature);
    FT_OpenType_Free(t.face, nullptr);
    FT_Bytes tables[FT_VALIDATE_GX_LENGTH]{};
    EXPECT_EQ(FT_TrueTypeGX_Validate(t.face, FT_VALIDATE_GX, tables, FT_VALIDATE_GX_LENGTH),
              FT_Err_Unimplemented_Feature);
    FT_TrueTypeGX_Free(t.face, nullptr);
    FT_Bytes kern_table{};
    EXPECT_EQ(FT_ClassicKern_Validate(t.face, FT_VALIDATE_MS, &kern_table), FT_Err_Unimplemented_Feature);
    FT_ClassicKern_Free(t.face, nullptr);
    // Logging is compiled out (no FT_DEBUG_LOGGING): the entry points are no-ops.
    FT_Trace_Set_Level("any:7");
    FT_Trace_Set_Default_Level();
    FT_Set_Log_Handler(nullptr);
    FT_Set_Default_Log_Handler();
}
