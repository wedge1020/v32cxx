#pragma once
// *****************************************************************************
//  v32/time.hpp — pilot C++ veneer over Vircon32's "time.h"
//
//  Same consumption model as v32/video.hpp and v32/input.hpp: inlined by
//  v32pp into the user's translation unit; the "#include" below is a
//  pass-through line resolved by the downstream Vircon32 C compiler.
//  Nothing here re-implements hardware access — every counter read and
//  every frame wait goes through the C API (get_cycle_counter,
//  get_frame_counter, get_date, get_time, end_frame, sleep).
//
//  Design notes:
//   - One-word params/returns only, as in every v32 header. The compound
//     date/time results use out-param and class-holder patterns (like
//     v32::Point), never a by-value struct return.
//   - time.h's own translate_date/translate_time are NOT called, because
//     they take date_info*/time_info* and the v32c++ subset cannot name C
//     struct types in source. Instead the same pure-math conversion is
//     expressed as private methods on the classes below — this is plain
//     arithmetic (no hardware access), matching the pilot rule that only
//     hardware access must pass through the C API.
//   - Braced array initializer lists are used freely (int a[3] = {1,2,3}),
//     matching the C headers' own style; the grammar's opt_array_initializer
//     accepts them for both declarator spellings, and string-literal
//     initializers (int msg[8] = "Hello") are equally supported.
//   - Ternaries are used freely where they read naturally: the transpiler
//     rewrites them (if/else for statement positions, hoisted temps for
//     expression positions) so the Vircon32 C compiler never sees one.
// *****************************************************************************

#include "time.h"

namespace v32
{
    // -------------------------------------------------------------------------
    //  Typed constant for the console's frame rate. The C header's
    //  #define (frames_per_second) is still available pass-through; this
    //  enum gives the value a proper name at the v32 layer, the same way
    //  video.hpp typed the blending-mode hex constants.
    // -------------------------------------------------------------------------

    enum FrameTiming
    {
        FramesPerSecond = 60
    };

    // -------------------------------------------------------------------------
    //  Date — human-readable calendar date, filled from the hardware clock.
    //
    //  Storage is three plain ints, so it passes by value fine word-by-word,
    //  but per convention it is passed by pointer/reference at the API.
    // -------------------------------------------------------------------------

    class Date
    {
        public:
            int year;
            int month;   // 1 (January) to 12 (December)
            int day;      // starting from 1

            Date()
            {
                year = 0;
                month = 1;
                day = 1;
            }

            // read the hardware clock and convert in one call
            void set_now()
            {
                set_from( get_date() );
            }

            // convert a raw Vircon date value (year << 16 | days elapsed)
            // same math as time.h's translate_date, expressed subset-safe
            void set_from( int date_value )
            {
                year = date_value >> 16;
                int days_in_year = date_value & 0x0000FFFF;

                int month_days[ 12 ] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

                // leap years: Feb gets 29 days (same rule as time.h)
                bool is_leap_year = ((year % 4) == 0) && ((year % 100) != 0);
                if( is_leap_year )
                  month_days[ 1 ] = 29;

                month = 1;
                for( int m = 0; m < 11; ++m )
                {
                    if( days_in_year < month_days[ m ] )
                    {
                        day = days_in_year + 1;
                        return;
                    }
                    days_in_year -= month_days[ m ];
                    month = month + 1;
                }

                // if this is reached, the date is in December
                month = 12;
                day = days_in_year + 1;
            }
    };

    // -------------------------------------------------------------------------
    //  TimeOfDay — hours/minutes/seconds from the hardware clock.
    //  Same conversion math as time.h's translate_time.
    // -------------------------------------------------------------------------

    class TimeOfDay
    {
        public:
            int hours;     // 0 to 23
            int minutes;   // 0 to 59
            int seconds;   // 0 to 59

            TimeOfDay()
            {
                hours = 0;
                minutes = 0;
                seconds = 0;
            }

            void set_now()
            {
                set_from( get_time() );
            }

            void set_from( int seconds_of_day )
            {
                hours = seconds_of_day / 3600;
                minutes = (seconds_of_day % 3600) / 60;
                seconds = seconds_of_day % 60;
            }
    };

    // -------------------------------------------------------------------------
    //  Stopwatch — frame-counter-based elapsed time, the v32 replacement
    //  for "how long has it been" patterns. Mutating, zero allocation.
    // -------------------------------------------------------------------------

    class Stopwatch
    {
        int start_frame;

        public:
            Stopwatch()
            {
                start_frame = get_frame_counter();
            }

            void restart()
            {
                start_frame = get_frame_counter();
            }

            int elapsed_frames()
            {
                return get_frame_counter() - start_frame;
            }

            // elapsed frames as a float fraction of seconds; the int
            // widens implicitly, no cast needed (subset-avoidance)
            float elapsed_seconds()
            {
                return elapsed_frames() * frame_time;
            }
    };

    // -------------------------------------------------------------------------
    //  FrameScope — RAII frame boundary, the TextureScope pattern applied to
    //  timing: declare it at the top of a per-frame block and the frame ends
    //  at the closing brace, even on an early return. No exceptions exist in
    //  this language, so this is complete RAII for the subset.
    // -------------------------------------------------------------------------

    class FrameScope
    {
        public:
            FrameScope()
            {
            }

            ~FrameScope()
            {
                end_frame();
            }
    };

    // usage:
    //
    //     void main()
    //     {
    //         while( true )
    //         {
    //             v32::FrameScope frame;   // frame ends at the loop brace
    //             update();
    //             draw();
    //         }
    //     }
    //
    // -------------------------------------------------------------------------

    // -------------------------------------------------------------------------
    //  Small conversion helpers. frames_per_second and frame_time are the
    //  C header's #defines, used as pass-through identifiers exactly like
    //  screen_width in video.hpp's screen_center.
    // -------------------------------------------------------------------------

    // frames -> seconds; int widens to float implicitly
    float frames_to_seconds( int frames )
    {
        return frames * frame_time;
    }

    // seconds -> whole frames; the implicit float->int narrowing is the
    // C compiler's own conversion, kept deliberate and documented rather
    // than hidden behind a cast spelling the subset may not support
    int seconds_to_frames( float seconds )
    {
        return seconds * frames_per_second;
    }

    // wait a fractional number of seconds (sleep() itself takes frames)
    void wait_seconds( float seconds )
    {
        sleep( seconds_to_frames( seconds ) );
    }
}
