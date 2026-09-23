// *****************************************************************************
//  tests/89sample.cpp — smoke test for the v32/time.hpp pilot header
//
//  Build model: same as 88sample — v32pp runs first, inlining the .hpp files
//  into this translation unit (passing their inner `#include "time.h"` /
//  `#include "video.h"` lines through for the Vircon32 C compiler), then
//  v32c++ transpiles the expanded source.
//
//  Include order: video.hpp / string.hpp BEFORE time.hpp (string.hpp is
//  mandatory anyway per 88sample's TYPE_NAME-feedback note; time.hpp does
//  not depend on Point, but printing needs v32::String).
//
//  Deliberately exercised here:
//   1. v32::FrameScope RAII guard — the third scope-guard class, and the
//      first one with a NON-TRIVIAL destructor dependency: ~FrameScope
//      calls end_frame(), a real C function, so the dtor walk must lower
//      a call, not just a state restore. Both exit paths proven like
//      87/88sample: fall-off-the-end and an early return from a helper.
//   2. v32::Date::set_from — the braced array initializer list
//      (`int month_days[12] = {...}`) from inside a v32:: header, plus
//      BOTH year shapes: a non-leap date AND a leap-year date (Feb 29),
//      so month_days[1] = 29's conditional assignment actually runs.
//   3. v32::TimeOfDay::set_from — plain arithmetic conversion, checked
//      against known values (e.g. 3661 seconds -> 1:01:01).
//   4. v32::Stopwatch — the FIRST float-returning method in any v32::
//      header (elapsed_seconds), plus restart() and int-returning
//      elapsed_frames(). The float return through a mangled call is new
//      ground for the pilot headers.
//   5. Free helpers frames_to_seconds / seconds_to_frames — including the
//      deliberate implicit float->int narrowing in seconds_to_frames,
//      observed rather than hidden; and wait_seconds wrapping sleep().
//   6. Ternaries in expression position (line += cond ? "1" : "0") per the
//      corrected pilot stance, coexisting with raw C API calls
//      (get_frame_counter(), end_frame(), sleep()) in the same unit.
//   7. String-literal array initializer (`int msg[8] = "Hi";`) used
//      directly by sample code — the opt_array_initializer STRING_LITERAL
//      grammar path, exercised in a test file for the first time.
//   8. v32::FramesPerSecond enum constant flowing into real arithmetic.
//
//  Deliberately NOT exercised (known boundaries, not silently missed):
//   - the C struct types date_info/time_info (unnamable in the subset;
//     time.h's own translate_date/translate_time are NOT called anywhere)
//   - FrameTiming's float frame_time beyond multiplication (no float
//     globals, no float params — one-word constraint stays intact)
// *****************************************************************************

// ---- pilot headers (v32pp inlines these) ------------------------------------
#include "v32/video.hpp"
#include "v32/string.hpp"
#include "v32/time.hpp"

// ---- C headers used directly by this sample's own code ----------------------
#include "time.h"

// ---- global state -----------------------------------------------------------

int loop_frames = 0;

// -----------------------------------------------------------------------------
//  Early-return path through a FrameScope: ~FrameScope must fire (calling
//  end_frame) BEFORE the return leaves the function. Mirrors 87sample's
//  TextureScope and 88sample's GamepadScope checks, dtor-with-a-call side.
// -----------------------------------------------------------------------------

int early_exit_inside_frame_scope( int threshold )
{
    v32::FrameScope frame;   // frame ends when this scope does

    if( loop_frames > threshold )
    {
        return 1;  // dtor (end_frame) must run here
    }

    return 0;      // and here
}

// -----------------------------------------------------------------------------
//  Date conversion checks, both year shapes. Prints the results so the
//  generated output can be read, not just trusted to compile.
// -----------------------------------------------------------------------------

void print_date_test( int y, int x )
{
    v32::String line;

    // non-leap year 2023: day index 59 is March 1st (Jan 1 = index 0,
    // so Feb 28 = index 58 in a NON-leap year)
    v32::Date d;
    d.set_from( (2023 << 16) | 59 );
    line = "2023 day 60: ";
    line.append_int( d.year, 10 );
    line += "-";
    line.append_int( d.month, 10 );
    line += "-";
    line.append_int( d.day, 10 );
    line.print_at_xy( y, x );

    // leap year 2024: same index 59 is Feb 29th -- the ONLY index where
    // the two years diverge, so month_days[1] = 29 genuinely decides it
    v32::Date leap;
    leap.set_from( (2024 << 16) | 59 );
    line = "2024 day 60: ";
    line.append_int( leap.year, 10 );
    line += "-";
    line.append_int( leap.month, 10 );
    line += "-";
    line.append_int( leap.day, 10 );
    line.print_at_xy( y, x + 20 );
}

// -----------------------------------------------------------------------------

void main()
{
	clear_screen (color_black);

    // -- Date: hardware clock + both conversion shapes -----------------------
    print_date_test( 20, 20 );

    v32::Date now;
    now.set_now();
    v32::String line;
    line = "today: ";
    line.append_int( now.year, 10 );
    line += "-";
    line.append_int( now.month, 10 );
    line += "-";
    line.append_int( now.day, 10 );
    line.print_at_xy( 20, 80 );

    // -- TimeOfDay: known-value conversion + hardware clock ------------------
    v32::TimeOfDay t;
    t.set_from( 3661 );   // 1 hour, 1 minute, 1 second
    line = "3661s -> ";
    line.append_int( t.hours, 10 );
    line += ":";
    line.append_int( t.minutes, 10 );
    line += ":";
    line.append_int( t.seconds, 10 );
    line.print_at_xy( 20, 100 );
    // generated output must read 1:1:1

    v32::TimeOfDay clock_now;
    clock_now.set_now();
    line = "clock: ";
    line.append_int( clock_now.hours, 10 );
    line += ":";
    line.append_int( clock_now.minutes, 10 );
    line += ":";
    line.append_int( clock_now.seconds, 10 );
    line.print_at_xy( 20, 120 );

    // -- Stopwatch: first float-returning method in a v32:: header ----------
    v32::Stopwatch watch;
    sleep( 3 );   // raw C API: 3 frames pass

    line = "elapsed frames: ";
    line.append_int( watch.elapsed_frames(), 10 );
    line.print_at_xy( 20, 140 );

    // float seconds (append_float takes NO precision arg -- ftoa's
    // format is fixed by the C API)
    line = "elapsed seconds: ";
    line.append_float( watch.elapsed_seconds() );
    line.print_at_xy( 20, 160 );

    // restart resets the base; a raw ternary in expression position
    watch.restart();
    line = "after restart: ";
    line += (watch.elapsed_frames() == 0) ? "zero" : "moved";
    line.print_at_xy( 20, 180 );

    // -- free helpers, including the deliberate narrowing -------------------
    float fps = v32::frames_to_seconds( v32::FramesPerSecond );
    line = "1 second = ";
    line.append_float( fps );
    line.print_at_xy( 20, 200 );
    // generated output must read 1 (ftoa's format is the C API's own)

    int frames_back = v32::seconds_to_frames( 0.5 );
    line = "0.5s in frames: ";
    line.append_int( frames_back, 10 );
    line.print_at_xy( 20, 220 );
    // generated output must read 30 (60 * 0.5, narrowed by the C compiler)

    // -- string-literal array initializer, sample-side ---------------------
    int msg[ 8 ] = "Hi";
    line = "msg len: ";
    line.append_int( strlen( msg ), 10 );
    line.print_at_xy( 20, 240 );
    // generated output must read 2

    // -- early-return FrameScope, called for real ---------------------------
    int exited = early_exit_inside_frame_scope( 1000 );
    line = "early exit: ";
    line.append_int( exited, 10 );
    line.print_at_xy( 20, 260 );

    // -- frame loop with FrameScope + wait_seconds ---------------------------
    while( 1 )
    {
        v32::FrameScope frame;   // end_frame at the brace, dtor-with-call

        loop_frames = loop_frames + 1;
        if( loop_frames == 30 )
        {
            v32::wait_seconds( 0.25 );   // ~15 frames via the veneer
        }
    }
}
