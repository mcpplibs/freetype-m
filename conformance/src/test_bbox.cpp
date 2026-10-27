// FreeType 2.13.3 src/tools/test_bbox.c, ported onto
// import mcpplibs.mwrap.freetype: C++ for C, the module for the headers,
// constexpr functions for the XX / XVEC / XVAL macros, std::chrono for
// clock() (whose CLOCKS_PER_SEC is a macro), the outline arrays in the
// 2.13 FT_Outline field types (unsigned char tags, unsigned short contours),
// and the %08X arguments as the unsigned values they print. The outlines,
// the computations and the output are upstream's.
//
// Copyright (C) The FreeType Project; part of FreeType, used under the
// FreeType License (upstream/freetype/docs/FTL.TXT).
import std;
import mcpplibs.mwrap.freetype;

using namespace mcpplibs::mwrap::freetype::macros;

  static long
  get_time( void )
  {
    using namespace std::chrono;
    return static_cast<long>(
      duration_cast<microseconds>( steady_clock::now().time_since_epoch() ).count() / 100 );
  }




  /* test bbox computations */

constexpr long  XSCALE = 65536;
constexpr FT_Pos     XX( double  x )             { return (FT_Pos)( x * XSCALE ); }
constexpr FT_Vector  XVEC( double  x, double  y ) { return { XX( x ), XX( y ) }; }
constexpr double     XVAL( FT_Pos  x )           { return x / ( 1.0 * XSCALE ); }

  /* dummy outline #1 */
  static FT_Vector  dummy_vec_1[4] =
  {
    XVEC( 408.9111, 535.3164 ),
    XVEC( 455.8887, 634.396  ),
    XVEC( -37.8765, 786.2207 ),
    XVEC( 164.6074, 535.3164 )
  };

  static unsigned char  dummy_tag_1[4] =
  {
    FT_CURVE_TAG_ON,
    FT_CURVE_TAG_CUBIC,
    FT_CURVE_TAG_CUBIC,
    FT_CURVE_TAG_ON
  };

  static unsigned short  dummy_contour_1[1] =
  {
    3
  };

  static FT_Outline  dummy_outline_1 =
  {
    1,
    4,
    dummy_vec_1,
    dummy_tag_1,
    dummy_contour_1,
    0
  };


  /* dummy outline #2 */
  static FT_Vector  dummy_vec_2[4] =
  {
    XVEC( 100.0, 100.0 ),
    XVEC( 100.0, 200.0 ),
    XVEC( 200.0, 200.0 ),
    XVEC( 200.0, 133.0 )
  };

  static FT_Outline  dummy_outline_2 =
  {
    1,
    4,
    dummy_vec_2,
    dummy_tag_1,
    dummy_contour_1,
    0
  };


  /* dummy outline #3 with bbox of [0 100 128 128] precisely */
  static FT_Vector  dummy_vec_3[4] =
  {
    XVEC( 100.0, 127.0 ),
    XVEC( 200.0, 127.0 ),
    XVEC(   0.0, 136.0 ),
    XVEC(   0.0, 100.0 )
  };

  static FT_Outline  dummy_outline_3 =
  {
    1,
    4,
    dummy_vec_3,
    dummy_tag_1,
    dummy_contour_1,
    0
  };


  [[maybe_unused]] static void
  dump_outline( FT_Outline*  outline )
  {
    FT_BBox  bbox;

    /* compute and display cbox */
    FT_Outline_Get_CBox( outline, &bbox );
    std::printf( "cbox = [%.2f %.2f %.2f %.2f]\n",
                 XVAL( bbox.xMin ),
                 XVAL( bbox.yMin ),
                 XVAL( bbox.xMax ),
                 XVAL( bbox.yMax ) );

    /* compute and display bbox */
    FT_Outline_Get_BBox( outline, &bbox );
    std::printf( "bbox = [%.2f %.2f %.2f %.2f]\n",
                 XVAL( bbox.xMin ),
                 XVAL( bbox.yMin ),
                 XVAL( bbox.xMax ),
                 XVAL( bbox.yMax ) );
  }



  static void
  profile_outline( FT_Outline*   outline,
                   long          repeat )
  {
    FT_BBox  bbox;
    long     count;
    long     time0;

    time0 = get_time();
    for ( count = repeat; count > 0; count-- )
      FT_Outline_Get_CBox( outline, &bbox );

    time0 = get_time() - time0;
    std::printf( "time = %6.3f cbox = [%8.4f %8.4f %8.4f %8.4f]\n",
                 ((double)time0/10000.0),
                 XVAL( bbox.xMin ),
                 XVAL( bbox.yMin ),
                 XVAL( bbox.xMax ),
                 XVAL( bbox.yMax ) );
    std::printf( "cbox_hex = [%08X %08X %08X %08X]\n",
                 (unsigned)bbox.xMin, (unsigned)bbox.yMin,
                 (unsigned)bbox.xMax, (unsigned)bbox.yMax );


    time0 = get_time();
    for ( count = repeat; count > 0; count-- )
      FT_Outline_Get_BBox( outline, &bbox );

    time0 = get_time() - time0;
    std::printf( "time = %6.3f bbox = [%8.4f %8.4f %8.4f %8.4f]\n",
                 ((double)time0/10000.0),
                 XVAL( bbox.xMin ),
                 XVAL( bbox.yMin ),
                 XVAL( bbox.xMax ),
                 XVAL( bbox.yMax ) );
    std::printf( "bbox_hex = [%08X %08X %08X %08X]\n",
                 (unsigned)bbox.xMin, (unsigned)bbox.yMin,
                 (unsigned)bbox.xMax, (unsigned)bbox.yMax );
  }

constexpr long  REPEAT = 1000000L;

  int  main( int  argc, char**  argv )
  {
    (void)argc;
    (void)argv;

    std::printf( "outline #1\n" );
    profile_outline( &dummy_outline_1, REPEAT );

    std::printf( "outline #2\n" );
    profile_outline( &dummy_outline_2, REPEAT );

    std::printf( "outline #3\n" );
    profile_outline( &dummy_outline_3, REPEAT );

    return 0;
  }
