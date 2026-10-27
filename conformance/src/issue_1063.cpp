// FreeType 2.13.3 tests/issue-1063/main.c, ported onto
// import mcpplibs.mwrap.freetype: C++ for C, the module for the headers, a
// std::string for the fixed path buffer, std::print to std::cerr for
// fprintf to stderr (a macro). The logic and output are upstream's.
//
// Copyright (C) The FreeType Project; part of FreeType, used under the
// FreeType License (upstream/freetype/docs/FTL.TXT).
import std;
import mcpplibs.mwrap.freetype;

using namespace mcpplibs::mwrap::freetype::macros;

int
main( void )
{
  FT_Library  library;
  FT_Face     face = nullptr;

  /*
   * We assume that `FREETYPE_TESTS_DATA_DIR` was set by `meson test`.
   * Otherwise we default to `../tests/data`.
   *
   * TODO (David): Rewrite this to pass the test directory through the
   * command-line.
   */
  const char*  testdata_dir = std::getenv( "FREETYPE_TESTS_DATA_DIR" );
  std::string  filepath     = std::string( testdata_dir ? testdata_dir
                                                        : "../tests/data" ) +
                              "/" + "As.I.Lay.Dying.ttf";


  FT_Init_FreeType( &library );
  if ( FT_New_Face( library, filepath.c_str(), 0, &face ) != 0 )
  {
    std::print( std::cerr, "Could not open file: {}\n", filepath );
    return 1;
  }

  for ( FT_ULong  i = 59; i < 171; i++ )
  {
    FT_UInt   gid  = FT_Get_Char_Index( face, i );
    FT_Error  code = FT_Load_Glyph( face, gid, FT_LOAD_DEFAULT );


    if ( code )
      std::printf( "unknown %d for char %lu, gid %u\n", code, i, gid );
  }

  return 0;
}

/* EOF */
