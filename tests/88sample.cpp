// *****************************************************************************
//  tests/88sample.cpp — smoke test for the v32/input.hpp pilot header
//
//  Build model: same as 87sample — v32pp runs first, inlining the .hpp files
//  into this translation unit (passing their inner `#include "input.h"` /
//  `#include "video.h"` lines through for the Vircon32 C compiler), then
//  v32c++ transpiles the expanded source.
//
//  Include order matters: video.hpp BEFORE input.hpp, because input.hpp's
//  direction() reuses v32::Point and does not re-declare it.
//
//  Deliberately exercised here:
//   1. v32::GamepadScope RAII guard — the second scope-guard class, and the
//      first one guarding INPUT-side state: save get_selected_gamepad(),
//      select the wanted pad, restore at scope exit. Both exit paths are
//      proven the same way 87sample proved TextureScope: fall-off-the-end
//      and an early return from the same helper function.
//   2. v32::direction(Point*) — the first place the veneer takes the ADDRESS
//      OF A MEMBER EXPRESSION (&out->x / &out->y inside the header). The
//      AST_UNOP addr case has only ever received plain AST_IDENTs from the
//      pilot headers before; this sample runs it through a call.
//   3. v32::Button enum + switch dispatch — button_frames_held()/pressed()
//      exercise AST_SWITCH with AST_CASE labels in a v32:: header for the
//      first time. Every case RETURNS (no fall-through), plus a trailing
//      return 0 after the switch — the no-default-case shape the header
//      chose on purpose.
//   4. A for-loop ITERATING the enum: all seven Button values driven
//      through pressed() in sequence, proving the enum's ints flow into
//      the switch intact and that the flat case-label list emits in
//      source order.
//   5. The plain bool readers (left/right/up/down/connected) alongside the
//      raw C API called directly (gamepad_left() as int) — both spellings
//      must resolve, in the same translation unit.
//   6. Nested scopes: GamepadScope INSIDE a TextureScope block, restoring
//      in reverse order — the scope-chain dtor walk with two different
//      guard types live at once.
//
//  Deliberately NOT exercised (known boundaries, not silently missed):
//   - switch with actual fall-through between cases (nothing in the header
//     needs it yet; a fall-through sample belongs in a dedicated lower.c
//     test, not a veneer smoke test)
//   - gamepad_direction_normalized() (needs a float Point; unwrapped)
// *****************************************************************************

// ---- pilot headers (v32pp inlines these; lines below are what it expands) --
#include "v32/video.hpp"
#include "v32/string.hpp"
#include "v32/input.hpp"

// ---- C headers used directly by this sample's own code ----------------------
// (end_frame lives in time.h; the pilot headers don't pull it in)
#include "time.h"

// ---- C headers called directly (raw-API coexistence proof) ------------------
#include "input.h"
#include "video.h"

// ---- global state for the frame loop ----------------------------------------

int frame_count = 0;

// -----------------------------------------------------------------------------
//  Early-return path through a GamepadScope: the destructor must fire BEFORE
//  the return leaves the function, restoring the previously selected pad.
//  (Mirrors draw_health_warning_or_return from 87sample, input-side.)
// -----------------------------------------------------------------------------

int early_exit_while_pad_selected( int pad, int threshold )
{
    v32::GamepadScope scope( pad );

    if( frame_count > threshold )
    {
        return 1;  // dtor must run here
    }

    return 0;      // and here
}

// -----------------------------------------------------------------------------
//  A tiny on-screen status line built from input state. Uses the bool
//  readers, one button through the enum dispatch, and a raw C call, all in
//  the same function.
// -----------------------------------------------------------------------------

void print_pad_status( int x, int y )
{
    v32::String line;

    line = "L";
    line += v32::left() ? "1" : "0";
    line += "R";
    line += v32::right() ? "1" : "0";
    line += "U";
    line += v32::up() ? "1" : "0";
    line += "D";
    line += v32::down() ? "1" : "0";

    line += " A";
    line += v32::pressed( v32::ButtonA ) ? "1" : "0";
    line += " S";
    line += v32::pressed( v32::ButtonStart ) ? "1" : "0";

    // raw C API coexisting with the veneer in one function
    int raw_frames = gamepad_button_b();
    line += " b_raw=";
    line.append_int( raw_frames, 10 );

    line.print_at_xy( x, y );
}

// -----------------------------------------------------------------------------

void main()
{
    v32::String pad_line;
    pad_line = "pad connected: ";
    pad_line += v32::connected() ? "yes" : "no";
    pad_line.print_at_xy( 20, 20 );

    // -- GamepadScope, fall-off-the-end path --------------------------------
    v32::Point move;
    move.x = 0;
    move.y = 0;

    {
        v32::GamepadScope pad( 0 );     // select gamepad 0, restore at exit

        // FIRST member-address-of through the veneer: direction() forwards
        // (&out->x, &out->y) into the C gamepad_direction().
        v32::direction( (&move) );

        print_pad_status( 20, 60 );
    }
    // gamepad selection restored here

    // -- enum iteration drives the switch dispatch over all seven buttons ---
    // (no (v32::Button)b cast here: a qualified cast target does not parse
    // yet -- the cast production only takes a bare TYPE_NAME after '(' --
    // so the loop uses ints against the int-taking overload instead)
    v32::String held_line;
    held_line = "held frames: ";
    for( int b = 0; b <= 6; b++ )
    {
        int frames = v32::button_frames_held( b );

        if( b > 0 )
        {
            held_line += " ";
        }
        held_line.append_int( frames, 10 );
    }
    held_line.print_at_xy( 20, 100 );

    // enum-typed spellings still exercised: named constants through both
    // overloads, plus a v32::Button-typed local holding a constant (no
    // int->enum conversion needed when the initializer is a constant)
    v32::Button start_button = v32::ButtonStart;
    if( v32::pressed( start_button ) )
    {
        held_line = "start is held";
        held_line.print_at_xy( 20, 120 );
    }
    if( v32::button_frames_held( v32::ButtonY ) > 0 )
    {
        held_line = "y is held";
        held_line.print_at_xy( 20, 140 );
    }

    // -- nested guard scopes: TextureScope outside, GamepadScope inside -----
    {
        v32::TextureScope tex( 1 );     // select texture 1
        {
            v32::GamepadScope pad( 1 ); // select gamepad 1

            v32::Point delta;
            delta.x = 0;
            delta.y = 0;
            v32::direction( (&delta) );

            int dx = delta.x;
            print_xy( 20 + (dx * 40), 140, "nested scopes: pad moves text" );
        }
        // gamepad restored first (innermost scope first)
    }
    // texture restored second

    // -- early-return dtor path, called for real ----------------------------
    pad_line = "early exit check: ";
    int exited = early_exit_while_pad_selected( 0, 1000 );
    pad_line.append_int( exited, 10 );
    pad_line.print_at_xy( 20, 180 );

    // -- the print-at-current-point member, input side ----------------------
    pad_line = "at current point";
    pad_line.print_current_point();

    // -- frame loop ---------------------------------------------------------
    while( 1 )
    {
        end_frame();
        frame_count++;
    }
}
