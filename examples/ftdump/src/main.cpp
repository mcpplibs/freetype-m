// ftdump — describe a font file through import mcpplibs.mwrap.freetype: every
// face in it (or each named instance), its format and driver, flags,
// metrics, fixed sizes, character maps, variation axes and instances, SFNT
// names and tables, colour palettes, PostScript and BDF properties. In the
// spirit of FreeType's ft2demos ftdump, written against the module.
//
//   ftdump <font file>...
import std;
import mcpplibs.mwrap.freetype;

using namespace mcpplibs::mwrap::freetype::macros;

namespace {

std::string tag_name(FT_ULong tag) {
    std::string s;
    for (int shift = 24; shift >= 0; shift -= 8) {
        char c = static_cast<char>((tag >> shift) & 0xFF);
        s += std::isprint(static_cast<unsigned char>(c)) ? c : '?';
    }
    return s;
}

std::string name_text(const FT_SfntName& n) {
    std::string out;
    bool utf16 = n.platform_id == TT_PLATFORM_MICROSOFT || n.platform_id == TT_PLATFORM_APPLE_UNICODE;
    for (FT_UInt i = utf16 ? 1 : 0; i < n.string_len; i += utf16 ? 2 : 1) {
        unsigned char c = n.string[i];
        bool ascii = !utf16 || n.string[i - 1] == 0;
        out += ascii && c >= 0x20 && c < 0x7F ? static_cast<char>(c) : '?';
    }
    return out;
}

std::string flags(FT_Face face) {
    std::vector<std::string_view> on;
    const std::pair<long, std::string_view> table[] = {
        {FT_FACE_FLAG_SCALABLE, "scalable"},       {FT_FACE_FLAG_FIXED_SIZES, "fixed-sizes"},
        {FT_FACE_FLAG_FIXED_WIDTH, "fixed-width"}, {FT_FACE_FLAG_SFNT, "sfnt"},
        {FT_FACE_FLAG_HORIZONTAL, "horizontal"},   {FT_FACE_FLAG_VERTICAL, "vertical"},
        {FT_FACE_FLAG_KERNING, "kerning"},         {FT_FACE_FLAG_MULTIPLE_MASTERS, "multiple-masters"},
        {FT_FACE_FLAG_GLYPH_NAMES, "glyph-names"}, {FT_FACE_FLAG_HINTER, "hinter"},
        {FT_FACE_FLAG_CID_KEYED, "cid-keyed"},     {FT_FACE_FLAG_TRICKY, "tricky"},
        {FT_FACE_FLAG_COLOR, "color"},             {FT_FACE_FLAG_VARIATION, "variation"},
        {FT_FACE_FLAG_SVG, "svg"},                 {FT_FACE_FLAG_SBIX, "sbix"},
    };
    for (auto [bit, name] : table)
        if (face->face_flags & bit) on.push_back(name);
    std::string out;
    for (auto n : on) out += (out.empty() ? "" : " ") + std::string(n);
    return out;
}

void dump_face(FT_Library lib, FT_Face face) {
    std::println("  family     {} / {}", face->family_name ? face->family_name : "-", face->style_name ? face->style_name : "-");
    const char* ps = FT_Get_Postscript_Name(face);
    std::println("  format     {} (driver {}), PostScript name {}", FT_Get_Font_Format(face), FT_FACE_DRIVER_NAME(face),
                 ps ? ps : "-");
    std::println("  flags      {}{}{}", flags(face), face->style_flags & FT_STYLE_FLAG_ITALIC ? ", italic" : "",
                 face->style_flags & FT_STYLE_FLAG_BOLD ? ", bold" : "");
    std::println("  glyphs     {}, {} units per EM", face->num_glyphs, face->units_per_EM);
    if (FT_IS_SCALABLE(face))
        std::println("  metrics    ascender {}, descender {}, height {}, bbox [{} {} {} {}]", face->ascender, face->descender,
                     face->height, face->bbox.xMin, face->bbox.yMin, face->bbox.xMax, face->bbox.yMax);
    for (int i = 0; i < face->num_fixed_sizes; ++i)
        std::println("  size       {}x{} px", face->available_sizes[i].width, face->available_sizes[i].height);
    for (int i = 0; i < face->num_charmaps; ++i) {
        FT_CharMap cm = face->charmaps[i];
        std::println("  charmap    {} platform {} encoding {}{}", tag_name(cm->encoding), cm->platform_id, cm->encoding_id,
                     FT_IS_SFNT(face) ? std::format(" format {}", FT_Get_CMap_Format(cm)) : std::string());
    }
    if (FT_HAS_MULTIPLE_MASTERS(face)) {
        FT_MM_Var* mm = nullptr;
        if (!FT_Get_MM_Var(face, &mm)) {
            for (FT_UInt a = 0; a < mm->num_axis; ++a)
                std::println("  axis       {} {} .. {} .. {}", tag_name(mm->axis[a].tag), mm->axis[a].minimum / 65536.0,
                             mm->axis[a].def / 65536.0, mm->axis[a].maximum / 65536.0);
            std::println("  instances  {}", mm->num_namedstyles);
            FT_Done_MM_Var(lib, mm);
        }
    }
    FT_UInt names = FT_Get_Sfnt_Name_Count(face);
    for (FT_UInt i = 0; i < names; ++i) {
        FT_SfntName n{};
        if (!FT_Get_Sfnt_Name(face, i, &n) && n.name_id <= TT_NAME_ID_VERSION_STRING &&
            (n.platform_id == TT_PLATFORM_MICROSOFT && n.language_id == TT_MS_LANGID_ENGLISH_UNITED_STATES))
            std::println("  name {:<5} {}", n.name_id, name_text(n));
    }
    if (FT_IS_SFNT(face)) {
        FT_ULong count = 0;
        FT_Sfnt_Table_Info(face, 0, nullptr, &count);
        std::string tables;
        for (FT_UInt i = 0; i < count; ++i) {
            FT_ULong tag = 0, length = 0;
            if (!FT_Sfnt_Table_Info(face, i, &tag, &length)) tables += std::format(" {}:{}", tag_name(tag), length);
        }
        std::println("  tables    {}", tables);
        std::println("  embedding fsType {:#06x}", FT_Get_FSType_Flags(face));
    }
    FT_Palette_Data palettes{};
    if (FT_HAS_COLOR(face) && !FT_Palette_Data_Get(face, &palettes))
        std::println("  palettes   {} of {} entries", palettes.num_palettes, palettes.num_palette_entries);
    PS_FontInfoRec info{};
    if (!FT_Get_PS_Font_Info(face, &info))
        std::println("  postscript {} / {} / {}", info.full_name ? info.full_name : "-", info.family_name ? info.family_name : "-",
                     info.weight ? info.weight : "-");
    const char *encoding = nullptr, *registry = nullptr;
    if (!FT_Get_BDF_Charset_ID(face, &encoding, &registry))
        std::println("  bdf        charset {}-{}", registry ? registry : "-", encoding ? encoding : "-");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::println(std::cerr, "usage: ftdump <font file>...");
        return 2;
    }
    FT_Library lib{};
    if (FT_Init_FreeType(&lib)) return 1;
    int status = 0;
    for (int f = 1; f < argc; ++f) {
        FT_Face probe{};
        if (FT_Error e = FT_New_Face(lib, argv[f], -1, &probe)) {
            std::println(std::cerr, "{}: not a font FreeType can open (error {:#x})", argv[f], e);
            status = 1;
            continue;
        }
        const FT_Long faces = probe->num_faces;
        FT_Done_Face(probe);
        std::println("{}: {} face{}", argv[f], faces, faces == 1 ? "" : "s");
        for (FT_Long i = 0; i < faces; ++i) {
            FT_Face face{};
            if (FT_New_Face(lib, argv[f], i, &face)) continue;
            std::println("face {}", i);
            dump_face(lib, face);
            FT_Done_Face(face);
        }
    }
    FT_Done_FreeType(lib);
    return status;
}
