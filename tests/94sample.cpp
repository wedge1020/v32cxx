// *****************************************************************************
//  tests/94sample.cpp — smoke test for the four remaining Option 1 veneers:
//  v32/math.hpp, v32/audio.hpp, v32/misc.hpp, v32/memcard.hpp
//
//  Build model: v32c++'s own include resolution inlines the .hpp files
//  (make test passes -I . so <v32/...> resolves from the project root);
//  their inner "#include "x.h"" lines pass through to the Vircon32 C
//  compiler.
//
//  Deliberately exercised:
//   1. math.hpp overload sets, including the case that needed a sema fix
//      this round: an overloaded call where one argument's type is unknown
//      (a pass-through C call or #define) but another argument settles it
//      -- v32::minimum( rand(), 10 ), v32::clamp( x, 0, screen_width ).
//   2. audio.hpp: SoundScope/ChannelScope guards, and Channel methods
//      that return a value from inside a scope-guarded body (the return
//      value must be computed BEFORE ~ChannelScope runs).
//   3. misc.hpp: HeapBlock with an early return (destructor runs on both
//      exit paths), random helpers, word helpers.
//   4. memcard.hpp: the first real consumer of `native` -- game_signature
//      is declared native in the header and every signature call casts an
//      int[20] to game_signature* at the C boundary.
//   5. Two v32:: functions whose names are also used by this file at
//      file scope (clamp, halt) -- with namespace-qualified mangling the
//      two coexist; before it they collapsed into one C symbol.
// *****************************************************************************

#include <v32/math.hpp>
#include <v32/audio.hpp>
#include <v32/misc.hpp>
#include <v32/memcard.hpp>
#include "video.h"
#include "string.h"      // itoa, for the on-screen result

// same names as v32:: functions, deliberately (see point 5 above)
int clamp( int value, int low, int high )
{
    return value;   // intentionally NOT clamping: must never be called by v32::clamp users
}

void halt()
{
}

// ---- 1. math ----------------------------------------------------------------

int math_checks()
{
    int errors = 0;

    if( v32::minimum( 3, 9 ) != 3 ) errors++;
    if( v32::maximum( 3, 9 ) != 9 ) errors++;
    if( v32::absolute( -7 ) != 7 ) errors++;

    float f = v32::minimum( 1.5, 2.5 );
    if( f != 1.5 ) errors++;
    float g = v32::absolute( -2.5 );
    if( g != 2.5 ) errors++;

    // pass-through arguments settled by a known one
    int small = v32::minimum( rand(), 10 );
    if( small > 10 ) errors++;
    int x = 900;
    int on_screen = v32::clamp( x, 0, screen_width );
    if( on_screen != screen_width ) errors++;

    // file-scope clamp is a different function
    if( clamp( 900, 0, 10 ) != 900 ) errors++;

    if( v32::sign( -4 ) != -1 ) errors++;
    float half = v32::lerp( 0.0, 10.0, 0.5 );
    if( half != 5.0 ) errors++;
    float d = v32::distance( 0.0, 0.0, 3.0, 4.0 );
    if( d < 4.99 || d > 5.01 ) errors++;
    float quarter_turn = v32::to_radians( 90.0 );
    if( quarter_turn < 1.57 || quarter_turn > 1.58 ) errors++;

    return errors;
}

// ---- 2. audio ---------------------------------------------------------------

int audio_checks()
{
    int errors = 0;

    v32::set_sound_looping( 0, true );
    v32::set_sound_loop_points( 0, 0, 44100 );

    select_channel( 5 );                 // the caller's own selection...
    v32::Channel music( 0 );
    music.set_volume( 0.8 );
    music.set_looping( true );
    music.play( 0 );
    int state = music.state();
    if( get_selected_channel() != 5 ) errors++;   // ...survives every call

    if( music.number() != 0 ) errors++;
    if( state != v32::ChannelPlaying && state != v32::ChannelStopped ) errors++;

    int ch = v32::play_any( 0 );
    if( ch != v32::NoFreeChannel && (ch < 0 || ch >= v32::SoundChannels) ) errors++;
    if( get_selected_channel() != 5 ) errors++;

    music.stop();
    v32::set_master_volume( 1.0 );
    return errors;
}

// ---- 3. misc ----------------------------------------------------------------

int sum_of_block( int words, bool bail_early )
{
    v32::HeapBlock block( words );
    if( !block.ok() ) return -1;

    block.fill( 2 );
    if( bail_early ) return 0;           // ~HeapBlock must run here too

    int* data = block.data();
    int total = 0;
    for( int i = 0; i < block.size(); i++ )
      total += data[ i ];
    return total;                        // ...and here
}

int misc_checks()
{
    int errors = 0;

    v32::seed_random( -1 );              // degenerate seed, replaced with 1
    int roll = v32::random_between( 1, 6 );
    if( roll < 1 || roll > 6 ) errors++;
    if( v32::random_below( 10 ) >= 10 ) errors++;

    if( sum_of_block( 8, false ) != 16 ) errors++;
    if( sum_of_block( 8, true ) != 0 ) errors++;

    int a[ 4 ];
    int b[ 4 ];
    v32::fill_words( a, 7, 4 );
    v32::copy_words( b, a, 4 );
    if( !v32::same_words( a, b, 4 ) ) errors++;

    return errors;
}

// ---- 4. memory card ---------------------------------------------------------

int signature[ 20 ] = "V32CXX-SAMPLE-94";

int memcard_checks()
{
    int errors = 0;
    v32::MemoryCard card( signature );

    if( !card.connected() )
      return 0;                          // nothing more to check without a card

    if( card.is_blank() )
      card.claim();

    if( card.is_ours() )
    {
        int progress[ 3 ] = { 11, 22, 33 };
        int loaded[ 3 ];
        if( !card.save( progress, 0, 3 ) ) errors++;
        if( !card.load( loaded, 0, 3 ) ) errors++;
        if( !v32::same_words( progress, loaded, 3 ) ) errors++;

        // out-of-range requests are refused, not truncated
        if( card.save( progress, v32::CardDataWords - 1, 3 ) ) errors++;
    }

    int raw[ 20 ];
    v32::read_card_signature( raw );
    if( card.is_ours() && !v32::card_matches( raw ) ) errors++;

    return errors;
}

// -----------------------------------------------------------------------------

// Result word for a headless emulator run: -1 until main finishes, then
// the error count. A runner reads it from RAM after the CPU halts (its
// address is the compiler's `global_test_errors` define in the .asm).
int test_errors = -1;

void main()
{
    int errors = math_checks() + audio_checks() + misc_checks() + memcard_checks();
    test_errors = errors;

    int text[ 32 ];
    itoa( errors, text, 10 );
    clear_screen( color_black );
    print_at( 20, 20, "errors:" );
    print_at( 100, 20, text );

    halt();                              // file-scope halt(), not v32::halt()
    v32::halt();
}
