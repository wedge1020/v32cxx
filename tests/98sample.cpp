// *****************************************************************************
//  tests/98sample.cpp — macros and types from the Vircon32 SDK's own .h headers
//
//  .h headers still pass through to the generated C, but v32c++ now also
//  READS them (when it can find them -- see inc/config.h), so the C++ side
//  sees what they define:
//   1. their constants work where the parser needs a value: array sizes
//      (screen_width / bios_character_width) and #if conditions;
//   2. they're typed: pi and frame_time are floats, so float arithmetic
//      and overload resolution on them work;
//   3. their struct/typedef names are declared `native` automatically --
//      date_info and time_info are used here by pointer with no `native`
//      line anywhere in this file;
//   4. names survive into the output (int [(screen_width / ...)] columns).
//  make test points V32CXX_SDK_INCLUDE at tools/vircon32/bin/include (built
//  by tools/vircon32/build-tools.sh) and skips this sample without it.
// *****************************************************************************

#include "video.h"
#include "time.h"
#include "math.h"
#include "audio.h"
#include "string.h"

#ifndef screen_width
#error "SDK headers not found: set V32CXX_SDK_INCLUDE (see inc/config.h)"
#endif

int columns[ screen_width / bios_character_width ];     // 64
int channel_volume[ sound_channels ];                    // 16

float half_turn( float x ) { return x * 0.5; }
int   half_turn( int x )   { return x / 2; }

int test_errors = -1;

void main()
{
    int errors = 0;

    if( sizeof( columns ) != 64 )        errors++;
    if( sizeof( channel_volume ) != 16 ) errors++;

    float quarter = pi / 2;                     // float, not int division
    if( quarter < 1.57 || quarter > 1.58 )      errors++;

    float tick = frame_time * frames_per_second;
    if( tick < 0.99 || tick > 1.01 )            errors++;

    float h = half_turn( pi );                  // resolves to the float overload
    if( h < 1.57 || h > 1.58 )                  errors++;

#if screen_height == 360 && defined( color_red )
    int checked = 1;
#else
    int checked = 0;
#endif
    if( !checked )                              errors++;

    int storage[ 3 ];                           // native types are pointer-only
    date_info* now = (date_info*)&storage[ 0 ];  // no `native date_info;` needed:
    translate_date( get_date(), now );          // the type came from time.h
    if( storage[ 1 ] < 1 || storage[ 1 ] > 12 ) errors++;     // month

    if( color_red == color_blue )               errors++;
    if( INT_MAX != 2147483647 )                 errors++;

    test_errors = errors;

    int text[ 32 ];
    itoa( errors, text, 10 );
    clear_screen( color_black );
    print_at( 20, 20, "errors:" );
    print_at( 100, 20, text );
}
