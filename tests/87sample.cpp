// *****************************************************************************
//  tests/87sample.cpp — smoke test for the pilot v32/video.hpp + v32/string.hpp
//
//  Build model: v32pp runs first, inlining the two .hpp files into this
//  translation unit (and passing their inner `#include "video.h"` /
//  `#include "string.h"` lines through for the Vircon32 C compiler to
//  resolve), then v32c++ transpiles the expanded source.
//
//  Deliberately exercised here:
//   1. v32::String construction, append, operator+=, append_int/append_float,
//      comparison, printing.
//   2. v32::TextureScope RAII guard — on BOTH the fall-off-the-end path and
//      an early return (the two ways control can leave a block in this
//      language), proving phase-9 destructor emission per path.
//   3. v32::Point used only by reference / out-param — never by value
//      (the one-word lint pattern).
//   4. Ternaries in the positions the lowering phase supports: direct
//      assignment, direct return, and nested inside a call argument
//      (the __v32_tern_tmpN hoisting path). NO ternary inside a for-loop's
//      init/cond/incr clauses — that boundary is still open by design.
//   5. The enum-typed blending setter instead of a raw hex constant.
//   6. QUALIFIED function-as-value: `v32::`-qualified free functions used
//      as VALUES (function pointer init and assignment), exercising
//      finalize_calls_expr's AST_QUALIFIED_ID case — the qualified sibling
//      of the bare-name rewrite sample64 introduced. Both spellings are
//      tested: plain (`cb = v32::fn;`) and address-of (`&v32::fn`), since
//      the AST_UNOP case must recurse into the operand and hit the new
//      case exactly once (no double-wrap).
//
//  Deliberately NOT exercised (known boundaries, not silently missed):
//   - ternary in for-loop clauses (untouched by lowering today)
//   - any by-value class return (would trip the one-word limit)
// *****************************************************************************

// ---- pilot headers (v32pp inlines these; lines below are what it expands) --
#include "v32/video.hpp"
#include "v32/string.hpp"

// ---- C headers used directly by this sample's own code ----------------------
// (end_frame lives in time.h; the pilot headers don't pull it in)
#include "time.h"

// ---- global state for the frame loop ----------------------------------------

int frame_count = 0;
int player_score = 12345;
float player_health = 87.5;

// -----------------------------------------------------------------------------
//  early-return path through a TextureScope: the destructor must fire BEFORE
//  the return leaves the function (phase 9 walks the full scope chain,
//  innermost first)
// -----------------------------------------------------------------------------
int draw_health_warning_or_return( int x, int y )
{
    v32::TextureScope scope( -1 );   // bios texture, like print_at uses

    // ternary as a direct return expression (rewritten with no temp)
    return (player_health < 25.0) ? color_red : color_green;
}

void main( void )
{
    // ---- 1. String exercises -------------------------------------------------

    v32::String score_line( "Score: " );
    score_line.append_int( player_score, 10 );
    score_line += "  Health: ";
    score_line.append_float( player_health );

    // ternary assigned to a bare identifier (direct rewrite, no temp)
    int label_color = (frame_count > 0) ? color_yellow : color_white;

    // ternary NESTED inside a call argument (the hoisted-temp path)
    clear_screen( (player_health > 50.0) ? color_black : color_darkgray );

    // comparison operators on String
    v32::String a( "same" );
    v32::String b( "same" );
    v32::String c( "different" );
    bool ab_equal = (a == b.c_str());
    bool ac_equal = (a == c.c_str());
    bool ac_diff  = (a != c.c_str());

    // build a status line from the comparison results
    v32::String status( "ab_equal=" );
    status.append_int( ab_equal ? 1 : 0, 10 );
    status += " ac_equal=";
    status.append_int( ac_equal ? 1 : 0, 10 );
    status += " ac_diff=";
    status.append_int( ac_diff ? 1 : 0, 10 );

    // ---- 2. TextureScope on the fall-off-the-end path --------------------------

    {
        v32::TextureScope scope( -1 );
        select_region( 65 );  // 'A' region in the bios font
        set_drawing_point( 10, 40 );
        draw_region();
    }
    // bios texture still selected here? NO — the destructor must have
    // restored whatever was selected before the scope began.

    // early-return variant: exercises destructor-before-return emission
    int health_color = draw_health_warning_or_return( 0, 0 );

    // ---- 3. Point by reference / out-param only -------------------------------

    v32::Point center;
    v32::screen_center( &center );
    v32::Point offset( 0, 30 );
    center.add( offset );

    // ---- 4. typed blending enum ------------------------------------------------

    v32::set_blending( v32::BlendAlpha );

    // ---- 5. render it all ------------------------------------------------------

    print_at( center.x - 60, 10, score_line.c_str() );
    print_at( center.x - 60, center.y, status.c_str() );

    // ternary choosing between two print positions, nested in a call argument
    print_at( (frame_count % 2 == 0) ? 20 : 30, 340, "ternary-in-call works" );

    // the color chosen by the early-return function
    v32::String health_line( "health color id: " );
    health_line.append_int( health_color, 10 );
    health_line.print_at_xy( 20, 300 );

    // ---- 6. qualified function-as-value (function pointers) --------------------
    // exercise finalize_calls_expr's AST_QUALIFIED_ID rewrite: a v32::
    // qualified free function used as a VALUE must lower to its mangled
    // name, same as the bare-name form sample64 tests. Expected in the
    // generated C: the initializer/assignments read
    // screen_center__Point_ptr / draw__int_int_int (NOT bare
    // screen_center / draw), and both indirect calls run.

    // pointer to the qualified free function, initialized with the
    // plain qualified spelling
    void (*draw_fn)( int, int, int ) = v32::draw;
    draw_fn( 0, 400, 200 );   // indirect call through the pointer

    // assignment spelling, through a second pointer
    void (*draw_fn2)( int, int, int );
    draw_fn2 = &v32::draw;
    draw_fn2( 1, 420, 210 );

    // and the bare-name form alongside, to confirm it still works too
    void (*draw_fn3)( int, int, int ) = draw;
    draw_fn3( 2, 440, 220 );

    // ---- frame loop (no ternary in the for clauses — open boundary) -----------

    while( 1 )
    {
        end_frame();
        frame_count++;
    }
}
