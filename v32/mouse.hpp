#pragma once
// *****************************************************************************
//  v32/mouse.hpp — the v32mouse mouse driver, in C++
//
//  v32::Mouse (the v32io C library's mouse.h), built on the v32io core in
//  v32/v32io.hpp, which this includes. See that file for what the port
//  changes from the C library, and docs/V32IO.md for the full API, setup
//  and protocols.
//
//  !! HARDWARE REQUIREMENT !!
//  A v32io device reaches the console through a GAMEPAD PORT: it looks
//  like an ordinary gamepad, but its 11 controls carry keyboard or mouse
//  data. Something outside the console has to produce that data, so this
//  header does nothing useful on its own. One of these is REQUIRED:
//
//    - the v32io hardware adaptor (v32io-pico firmware on a modified
//      Waveshare RP2350-USB-A board), which plugs a real USB keyboard or
//      mouse in and presents it as the joystick `v32io:kbd` /
//      `v32io:mouse`; it works with ANY Vircon32 emulator, stock or not,
//      once a joystick profile maps button n to control n; or
//    - a modified Vircon32 DesktopEmulator offering the `v32kbd` and
//      `v32mouse` devices in its Gamepads menu: either the fork at
//      https://github.com/wedge1020/ComputerSoftware (its `v32io`
//      branch), or the stock emulator with the patches in the v32io
//      repository's emulator/ directory applied.
//
//  The STOCK emulator's own "Keyboard" device is NOT a v32io keyboard: it
//  maps keys to gamepad buttons, and a v32::Keyboard reading it sees
//  garbage. See docs/V32IO.md for setup, the protocols, and the examples
//  in demos/cxx/v32io.
//
//  THE RULE: call probe() once on EVERY frame for every device (keyboard
//  and mouse alike), before end_frame(). The console only updates the
//  gamepads between frames and a keyboard reports at most one key event
//  per frame: a skipped frame can lose a key, or misread mouse movement.
//  Extra probe() calls within one frame are harmless (they do nothing).
// *****************************************************************************

#include "v32io.hpp"

namespace v32
{
    // -------------------------------------------------------------------------
    //  Mouse buttons
    // -------------------------------------------------------------------------

    enum MouseButton
    {
        MouseLeft   = 1,
        MouseRight  = 2,
        MouseMiddle = 4,     // wheel click
        MouseAny    = 7
    };

    // meaning of the packed controls for a v32mouse device, and defaults
    enum MouseProtocol
    {
        MouseXNegative     = 1,      // IoLeft:  X trit, negative
        MouseXPositive     = 2,      // IoRight: X trit, positive
        MouseYNegative     = 4,      // IoUp:    Y trit, negative
        MouseYPositive     = 8,      // IoDown:  Y trit, positive
        MouseButtonMiddle  = 16,     // IoStart: middle button
        MouseButtonLeft    = 32,     // IoA:     left button
        MouseButtonRight   = 64,     // IoB:     right button
        MouseXHigh         = 128,    // IoX:     X Gray code, high bit
        MouseXLow          = 256,    // IoY:     X Gray code, low bit
        MouseYHigh         = 512,    // IoL:     Y Gray code, high bit
        MouseYLow          = 1024,   // IoR:     Y Gray code, low bit
        MousePositions     = 12,     // positions in a counter's cycle
        MouseDefaultScale  = 2,      // default pixels per step
        MouseScreenWidth   = 640,    // default pointer bounds: the screen
        MouseScreenHeight  = 360
    };

    // -------------------------------------------------------------------------
    //  Mouse counter decoding (v32mouse_counter / v32mouse_steps)
    //
    //  Movement is not sent as deltas but as two counters, one per axis,
    //  that go around a cycle of 12 positions; each step changes exactly
    //  one control. A counter is a 2-bit Gray code (4 groups) plus a "trit"
    //  on a pair of opposite directions (never both pressed: 3 states):
    //
    //      position:  0  1  2 | 3  4  5 | 6  7  8 | 9 10 11
    //      gray:        00    |   01    |   11    |   10
    //      trit:      -  0  + | +  0  - | -  0  + | +  0  -
    //
    //  At rest after power on, both counters sit at position 1 (nothing
    //  pressed at all).
    // -------------------------------------------------------------------------

    // position (0..11) of a counter in the packed controls, given the
    // masks of its 4 controls; -1 if they don't form a valid position
    int mouse_counter( int raw, int negative, int positive, int high, int low )
    {
        int trit  = 1;    // 0 negative, 1 none, 2 positive
        int group = 0;    // 0..3, from the Gray code

        if( (raw & high) != 0 )
        {
            group = 3;
            if( (raw & low) != 0 )
              group = 2;
        }
        else if( (raw & low) != 0 )
          group = 1;

        if( (raw & negative) != 0 )
        {
            if( (raw & positive) != 0 )
              return -1;    // both halves of the trit: not a position
            trit = 0;
        }
        else if( (raw & positive) != 0 )
          trit = 2;

        if( (group & 1) == 0 )
          return group * 3 + trit;
        return group * 3 + 2 - trit;    // odd groups walk the trit back
    }

    // movement (-5..+5 steps) from counter position `before` to `after`.
    // 6 can't be told apart from -6 and is taken as no movement; so is any
    // unknown (-1) position.
    int mouse_steps( int before, int after )
    {
        int steps = 0;

        if( before < 0 || after < 0 )
          return 0;

        steps = (after - before + MousePositions) % MousePositions;
        if( steps == 6 )
          return 0;
        if( steps > 6 )
          return steps - MousePositions;
        return steps;
    }

    // MouseButton bits held, from the packed controls
    int mouse_buttons( int raw )
    {
        int result = 0;
        if( (raw & MouseButtonLeft) != 0 )   result |= MouseLeft;
        if( (raw & MouseButtonRight) != 0 )  result |= MouseRight;
        if( (raw & MouseButtonMiddle) != 0 ) result |= MouseMiddle;
        return result;
    }

    // -------------------------------------------------------------------------
    //  Mouse — a v32mouse mouse on one gamepad port
    //
    //  Keeps a pointer, moved by (steps x scale) pixels per probe and held
    //  inside its bounds (the whole 640x360 screen by default, starting at
    //  its center, 2 pixels per step). Buttons are reported as held, and as
    //  "pressed"/"released" on the probe where they changed; the adaptor
    //  holds every button change for at least 25 ms, so even a very quick
    //  click lasts more than a frame. There is no scroll wheel: the 11
    //  controls have no room left for it.
    //
    //  Typical use:
    //
    //      v32::Mouse mouse( v32::SecondGamepadPort );
    //      while( true )
    //      {
    //          mouse.probe();                          // once per frame
    //          if( mouse.pressed( v32::MouseLeft ) )
    //            click_at( mouse.x(), mouse.y() );
    //          end_frame();
    //      }
    // -------------------------------------------------------------------------

    class Mouse
    {
        IoDevice io;
        int counter_x;         // last X counter position (-1: unknown)
        int counter_y;         // last Y counter position (-1: unknown)
        int steps_moved_x;     // movement in the latest probe, in steps
        int steps_moved_y;
        int pixels_moved_x;    // movement in the latest probe, in pixels
        int pixels_moved_y;
        int pointer_x;         // the pointer
        int pointer_y;
        int bound_min_x;       // pointer bounds (inclusive)
        int bound_min_y;
        int bound_max_x;
        int bound_max_y;
        int pixels_per_step;
        int held_buttons;      // MouseButton bits held
        int went_down;         // ... that went down in the latest probe
        int went_up;           // ... that went up in the latest probe
        int last_event_frame;  // frame of the latest movement/button change

        void keep_in_bounds()
        {
            if( pointer_x < bound_min_x ) pointer_x = bound_min_x;
            if( pointer_x > bound_max_x ) pointer_x = bound_max_x;
            if( pointer_y < bound_min_y ) pointer_y = bound_min_y;
            if( pointer_y > bound_max_y ) pointer_y = bound_max_y;
        }

        public:
            // The counters and buttons the device shows right now are the
            // starting point: movement is measured from here on.
            Mouse( int gamepad ) : io( gamepad )
            {
                int raw = 0;

                steps_moved_x    = 0;
                steps_moved_y    = 0;
                pixels_moved_x   = 0;
                pixels_moved_y   = 0;
                bound_min_x      = 0;
                bound_min_y      = 0;
                bound_max_x      = MouseScreenWidth - 1;
                bound_max_y      = MouseScreenHeight - 1;
                pointer_x        = MouseScreenWidth / 2;
                pointer_y        = MouseScreenHeight / 2;
                pixels_per_step  = MouseDefaultScale;
                went_down        = 0;
                went_up          = 0;
                last_event_frame = -1;

                raw          = io.read();
                counter_x    = mouse_counter( raw, MouseXNegative, MouseXPositive, MouseXHigh, MouseXLow );
                counter_y    = mouse_counter( raw, MouseYNegative, MouseYPositive, MouseYHigh, MouseYLow );
                held_buttons = mouse_buttons( raw );
            }

            // Call once per frame: works out the movement, moves the
            // pointer and updates the buttons. True if the mouse moved or
            // any button changed. On the frame the gamepad gets connected
            // the counters start over (no movement is taken from it).
            bool probe()
            {
                int raw     = 0;
                int buttons = 0;
                int new_x   = 0;
                int new_y   = 0;

                steps_moved_x  = 0;
                steps_moved_y  = 0;
                pixels_moved_x = 0;
                pixels_moved_y = 0;

                io.probe();
                raw = io.read();

                buttons      = mouse_buttons( raw );
                went_down    = buttons & ~held_buttons;
                went_up      = held_buttons & ~buttons;
                held_buttons = buttons;

                new_x = mouse_counter( raw, MouseXNegative, MouseXPositive, MouseXHigh, MouseXLow );
                new_y = mouse_counter( raw, MouseYNegative, MouseYPositive, MouseYHigh, MouseYLow );
                if( io.connected() && !io.plugged() )
                {
                    steps_moved_x = mouse_steps( counter_x, new_x );
                    steps_moved_y = mouse_steps( counter_y, new_y );
                }
                counter_x = new_x;
                counter_y = new_y;

                pixels_moved_x = steps_moved_x * pixels_per_step;
                pixels_moved_y = steps_moved_y * pixels_per_step;
                pointer_x += pixels_moved_x;
                pointer_y += pixels_moved_y;
                keep_in_bounds();

                if( steps_moved_x != 0 || steps_moved_y != 0 || went_down != 0 || went_up != 0 )
                {
                    last_event_frame = get_frame_counter();
                    return true;
                }
                return false;
            }

            // the pointer
            int x()
            {
                return pointer_x;
            }

            int y()
            {
                return pointer_y;
            }

            // both at once; either pointer may be NULL
            void position( int* out_x, int* out_y )
            {
                if( out_x != NULL ) *out_x = pointer_x;
                if( out_y != NULL ) *out_y = pointer_y;
            }

            // movement in the latest probe, in pixels (steps x scale) --
            // reported even where the pointer itself was stopped by its
            // bounds, so it also works for "mouse look" style controls
            int dx()
            {
                return pixels_moved_x;
            }

            int dy()
            {
                return pixels_moved_y;
            }

            // both at once; either pointer may be NULL
            void delta( int* out_dx, int* out_dy )
            {
                if( out_dx != NULL ) *out_dx = pixels_moved_x;
                if( out_dy != NULL ) *out_dy = pixels_moved_y;
            }

            // movement in the latest probe, in raw device steps (-5..+5)
            int steps_x()
            {
                return steps_moved_x;
            }

            int steps_y()
            {
                return steps_moved_y;
            }

            // place the pointer (kept within bounds)
            void set_position( int new_x, int new_y )
            {
                pointer_x = new_x;
                pointer_y = new_y;
                keep_in_bounds();
            }

            // area the pointer can move in, inclusive; the pointer is
            // pulled inside it right away
            void set_bounds( int min_x, int min_y, int max_x, int max_y )
            {
                bound_min_x = min_x;
                bound_min_y = min_y;
                bound_max_x = max_x;
                bound_max_y = max_y;
                keep_in_bounds();
            }

            // pixels per step (ignored unless positive)
            void set_scale( int scale )
            {
                if( scale > 0 )
                  pixels_per_step = scale;
            }

            int scale()
            {
                return pixels_per_step;
            }

            // is any button in the mask held? (MouseLeft, MouseAny, ...)
            bool is_down( int button )
            {
                return (held_buttons & button) != 0;
            }

            // did any button in the mask go down in the latest probe?
            bool pressed( int button )
            {
                return (went_down & button) != 0;
            }

            // did any button in the mask go up in the latest probe?
            bool released( int button )
            {
                return (went_up & button) != 0;
            }

            // every button held, as MouseButton bits
            int buttons()
            {
                return held_buttons;
            }

            // frame of the latest movement or button change, -1 before any
            int last_event()
            {
                return last_event_frame;
            }

            bool connected()
            {
                return io.connected();
            }

            int port()
            {
                return io.port();
            }

            // the underlying core, for looking at the raw packed controls
            IoDevice* device()
            {
                return &io;
            }
    };
}
