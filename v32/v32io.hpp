#pragma once
// *****************************************************************************
//  v32/v32io.hpp — v32io keyboard and mouse support, in C++
//
//  A C++ port of the v32io Vircon32 C drivers (v32io.h, keyboard.h and
//  mouse.h, https://github.com/wedge1020/v32io, lib/), for v32c++ programs.
//  Same protocols, same decoding, same once-per-frame rules; the C structs
//  and their v32io_* / v32kbd_* / v32mouse_* functions become three
//  classes:
//
//      v32::IoDevice    the core: reads a gamepad's 11 controls as data
//      v32::Keyboard    a v32kbd keyboard (key events, typed characters)
//      v32::Mouse       a v32mouse mouse (pointer, movement, buttons)
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
//  Consumption model: same as the other v32/ headers -- C++ source in the
//  v32c++ subset, inlined into the translation unit by v32c++'s own
//  include resolution (-I). The SDK "#include" lines below pass through to
//  the generated C. Unlike the other v32/ headers this one wraps no SDK
//  header of its own: the v32io C library is NOT needed to build a v32c++
//  program that uses it (everything is here), and the two are not meant to
//  be mixed in one program.
//
//  Differences from the C library, all deliberate:
//   - Constructors and member functions instead of *_init/*_free and
//     functions taking `v32kbd **`. A Keyboard/Mouse is an ordinary object
//     (a global, or a local in main) -- nothing to free.
//   - The keyboard's event queue is a fixed ring buffer inside the object
//     (64 events, like the C library's limit) instead of a malloc'd list:
//     no heap traffic on every key press, nothing to leak.
//   - The core's in-RAM routine is still generated once, by the
//     constructor, but the address it stores its result at is refreshed
//     on every scan, so an IoDevice that was copied keeps working.
//   - Constant names: V32IO_LEFT -> v32::IoLeft, V32KEY_LSHIFT ->
//     v32::KeyLShift, V32MOUSE_LEFT -> v32::MouseLeft,
//     FOURTH_GAMEPAD_PORT -> v32::FourthGamepadPort, and so on.
//
//  THE RULE: call probe() once on EVERY frame for every device (keyboard
//  and mouse alike), before end_frame(). The console only updates the
//  gamepads between frames and a keyboard reports at most one key event
//  per frame: a skipped frame can lose a key, or misread mouse movement.
//  Extra probe() calls within one frame are harmless (they do nothing).
// *****************************************************************************

#include "input.h"
#include "time.h"
#include "misc.h"

namespace v32
{
    // -------------------------------------------------------------------------
    //  Gamepad port ids (the C library's FIRST_GAMEPAD_PORT ... macros)
    // -------------------------------------------------------------------------

    enum GamepadPort
    {
        FirstGamepadPort  = 0,
        SecondGamepadPort = 1,
        ThirdGamepadPort  = 2,
        FourthGamepadPort = 3
    };

    // -------------------------------------------------------------------------
    //  The packed controls: one bit per gamepad control, in the order of
    //  the console's INP ports (bit n is port 0x402 + n)
    // -------------------------------------------------------------------------

    enum IoControl
    {
        IoLeft  = 1,      // bit  0: 0x402 INP_GamepadLeft
        IoRight = 2,      // bit  1: 0x403 INP_GamepadRight
        IoUp    = 4,      // bit  2: 0x404 INP_GamepadUp
        IoDown  = 8,      // bit  3: 0x405 INP_GamepadDown
        IoStart = 16,     // bit  4: 0x406 INP_GamepadButtonStart
        IoA     = 32,     // bit  5: 0x407 INP_GamepadButtonA
        IoB     = 64,     // bit  6: 0x408 INP_GamepadButtonB
        IoX     = 128,    // bit  7: 0x409 INP_GamepadButtonX
        IoY     = 256,    // bit  8: 0x40A INP_GamepadButtonY
        IoL     = 512,    // bit  9: 0x40B INP_GamepadButtonL
        IoR     = 1024,   // bit 10: 0x40C INP_GamepadButtonR
        IoAll   = 2047    // all 11 controls
    };

    enum IoLimits
    {
        IoControls      = 11,   // gamepad controls read
        IoRoutineWords  = 68,   // size of the in-RAM routine, in words
        IoRoutineTarget = 62    // routine word holding the result's address
    };

    // -------------------------------------------------------------------------
    //  IoDevice — a gamepad read as a v32io device (the v32io.h core)
    //
    //  Knows nothing of what the data means: it reads the 11 controls,
    //  packs them into one int (see IoControl) and keeps the packed
    //  controls of the latest probe and of the one before it. Keyboard and
    //  Mouse each own one; use it directly to build a driver of your own,
    //  or to look at what a device is sending (demos/cxx/v32io/monitor).
    //
    //  Reading 11 ports one C call at a time would cost 11 select/read
    //  round trips, so (exactly like the C library) the constructor writes
    //  a small machine-code routine into the object and scan() CALLs it:
    //
    //      PUSH R0 / PUSH R1 / PUSH R2
    //      MOV  R1, 0
    //      MOV  R2, R1
    //      11 x { IN R0, port   IGT R0, R2   SHL R0, bit   OR R1, R0 }
    //      MOV  [raw], R1
    //      POP R2 / POP R1 / POP R0
    //      RET
    //      HLT                                 (never reached; for safety)
    //
    //  IGT turns each port's "frames held" value (positive while pressed)
    //  into 0 or 1.
    // -------------------------------------------------------------------------

    class IoDevice
    {
        int  gamepad_id;      // gamepad being read
        int  raw;             // the routine stores its result here
        int  data;            // packed controls, latest probe
        int  previous;        // packed controls, the probe before that
        int  probe_frame;     // frame counter at the latest probe (-1: none)
        bool is_connected;    // gamepad connected, at the latest probe
        bool was_plugged;     // gamepad became connected at the latest probe
        int  routine[ IoRoutineWords ];

        public:
            IoDevice( int gamepad )
            {
                int index  = 0;
                int offset = 0;

                gamepad_id   = gamepad;
                raw          = 0;
                data         = 0;
                previous     = 0;
                probe_frame  = -1;
                is_connected = false;
                was_plugged  = false;

                routine[ 0 ] = 0x54000000;          // PUSH R0
                routine[ 1 ] = 0x54200000;          // PUSH R1
                routine[ 2 ] = 0x54400000;          // PUSH R2
                routine[ 3 ] = 0x4E200000;          // MOV  R1, 0
                routine[ 4 ] = 0x00000000;          //      (immediate)
                routine[ 5 ] = 0x4C424000;          // MOV  R2, R1

                for( index = 0; index < IoControls; index++ )
                {
                    offset = index * 5 + 6;
                    routine[ offset ]     = 0x5C000400 | (index + 2);  // IN  R0, 0x402+index
                    routine[ offset + 1 ] = 0x24040000;                // IGT R0, R2
                    routine[ offset + 2 ] = 0x96000000;                // SHL R0, index
                    routine[ offset + 3 ] = index;                     //     (immediate)
                    routine[ offset + 4 ] = 0x88200000;                // OR  R1, R0
                }

                // offset 61: index is IoControls (11) here
                offset = index * 5 + 6;
                routine[ offset ]     = 0x4E034000;     // MOV [raw], R1
                routine[ offset + 1 ] = (int)(&raw);    //     (address: IoRoutineTarget)
                routine[ offset + 2 ] = 0x58400000;     // POP R2
                routine[ offset + 3 ] = 0x58200000;     // POP R1
                routine[ offset + 4 ] = 0x58000000;     // POP R0
                routine[ offset + 5 ] = 0x10000000;     // RET
                routine[ offset + 6 ] = 0x00000000;     // HLT (for safety)

                // start from what the device is showing right now
                data     = scan();
                previous = data;
            }

            // the gamepad port (0-3) this device is read from
            int port()
            {
                return gamepad_id;
            }

            // Read the controls right now, without touching the probe
            // history (data/previous). Selects this device's gamepad for
            // the duration and restores the previous selection. Drivers use
            // probe() instead.
            int scan()
            {
                int previous_gamepad = 0;
                int entry            = 0;

                previous_gamepad = get_selected_gamepad();
                select_gamepad( gamepad_id );
                is_connected = gamepad_is_connected();

                // the object may have moved (been copied) since the
                // constructor wrote the routine: aim its store at our raw
                routine[ IoRoutineTarget ] = (int)(&raw);

                entry = (int)(&routine[ 0 ]);
                asm
                {
                    "PUSH  R0"
                    "MOV   R0, {entry}"
                    "CALL  R0"
                    "POP   R0"
                }

                select_gamepad( previous_gamepad );
                return raw;
            }

            // Call once per frame. Moves the latest packed controls to the
            // "previous" slot and reads new ones; further calls within the
            // same frame do nothing. True when the packed controls changed.
            bool probe()
            {
                int  frame  = 0;
                bool before = false;

                frame = get_frame_counter();
                if( frame == probe_frame )
                  return false;

                before      = is_connected;
                probe_frame = frame;
                previous    = data;
                data        = scan();
                was_plugged = is_connected && !before;
                return data != previous;
            }

            // packed controls from the latest probe
            int read()
            {
                return data;
            }

            // packed controls from the probe before the latest
            int read_previous()
            {
                return previous;
            }

            // controls that changed in the latest probe (IoControl bits)
            int changed()
            {
                return data ^ previous;
            }

            // is any control in mask held? (latest probe)
            bool is_down( int mask )
            {
                return (data & mask) != 0;
            }

            // gamepad connected? (latest probe)
            bool connected()
            {
                return is_connected;
            }

            // Did the gamepad become connected in the latest probe? A newly
            // connected gamepad shows every control released, whatever the
            // device is really showing, so protocols that depend on history
            // (the mouse counters) start over.
            bool plugged()
            {
                return was_plugged;
            }
    };

    // -------------------------------------------------------------------------
    //  Keyboard key codes
    //
    //  Codes identify KEYS, not characters. Keys with an ASCII character
    //  report it as typed with no shift on a US layout: 'a'..'z', '0'..'9',
    //  space and ` - = [ ] \ ; ' , . / -- compare those against character
    //  literals. The rest use these codes. The numeric keypad reports the
    //  same codes as the main keyboard. Code 0 is never reported.
    // -------------------------------------------------------------------------

    enum Key
    {
        KeyNone      = 0,
        KeyUp        = 1,     // arrow keys
        KeyDown      = 2,
        KeyLeft      = 3,
        KeyRight     = 4,
        KeyCapsLock  = 5,
        KeyLShift    = 6,
        KeyRShift    = 7,
        KeyBackspace = 8,
        KeyTab       = 9,
        KeyLCtrl     = 10,
        KeyRCtrl     = 11,
        KeyLAlt      = 12,    // left option on a Mac
        KeyEnter     = 13,
        KeyF1        = 14,    // F1..F12 are consecutive: KeyF1 + n - 1
        KeyF2        = 15,
        KeyF3        = 16,
        KeyF4        = 17,
        KeyF5        = 18,
        KeyF6        = 19,
        KeyF7        = 20,
        KeyF8        = 21,
        KeyF9        = 22,
        KeyF10       = 23,
        KeyF11       = 24,
        KeyF12       = 25,
        KeyRAlt      = 26,    // right option on a Mac
        KeyEscape    = 27,
        KeyLGui      = 28,    // left command on a Mac, Windows key on a PC
        KeyRGui      = 29,    // right command on a Mac
        KeyDelete    = 127
    };

    // meaning of the packed controls for a v32kbd device, and its limits
    enum KeyboardProtocol
    {
        KeyboardStrobe    = 3,     // IoLeft | IoRight: alternates per event
        KeyboardPressed   = 4,     // IoUp:   the event is a key press
        KeyboardReleased  = 8,     // IoDown: the event is a key release
        KeyboardCodeShift = 4,     // key code starts at IoStart (bit 4)
        KeyboardCodeMask  = 127,   // and is 7 bits long
        KeyboardKeys      = 128,   // possible key codes
        KeyboardQueueSize = 64     // events that can wait to be read
    };

    // one key event, as next_event() hands it out
    struct KeyEvent
    {
        int  key;        // key code (a character, or a Key constant)
        int  symbol;     // typed character: shift and caps lock applied as
                         // they were when the key was pressed (same as key
                         // for keys with no character)
        bool pressed;    // true: pressed, false: released
    };

    // -------------------------------------------------------------------------
    //  Keyboard — a v32kbd keyboard on one gamepad port
    //
    //  Protocol: the device reports at most one key event per frame and
    //  holds it until the next. The strobe (Left / Right) tells events
    //  apart: the first event presses Left, the next Right, then Left
    //  again, so a new event has arrived whenever the side differs from
    //  the last one seen (which is what lets the same key arrive twice in a
    //  row). Up = press, Down = release, Start/A/B/X/Y/L/R = the 7-bit key
    //  code. Shift and caps lock are tracked here to produce characters.
    //
    //  Typical use:
    //
    //      v32::Keyboard keyboard( v32::SecondGamepadPort );
    //      while( true )
    //      {
    //          keyboard.probe();                  // once per frame
    //          int key = keyboard.read();
    //          while( key > 0 )                   // every press this frame
    //          {
    //              ...
    //              key = keyboard.read();
    //          }
    //          end_frame();
    //      }
    //
    //  read() and read_event()/next_event() take from the same queue: use
    //  one style per keyboard. is_down() works alongside either.
    // -------------------------------------------------------------------------

    class Keyboard
    {
        IoDevice io;
        int  strobe_side;        // last strobe side seen (0 none, 1 L, 2 R)
        bool caps_lock;          // caps lock state (toggles on each press)
        int  last_event_frame;   // frame of the latest key event (-1: none)
        int  dropped_events;     // events lost to a full queue
        int  queue_first;        // ring buffer: index of the oldest event
        int  queue_count;        // ring buffer: events waiting
        int  queue_key[ KeyboardQueueSize ];
        int  queue_symbol[ KeyboardQueueSize ];
        bool queue_pressed[ KeyboardQueueSize ];
        bool held[ KeyboardKeys ];

        // drop the oldest queued event (caller checks queue_count > 0)
        void pop_event()
        {
            queue_first = (queue_first + 1) % KeyboardQueueSize;
            queue_count--;
        }

        public:
            // The device keeps the state of its last event even across a
            // console reset, so the current strobe side is taken as the
            // starting point: an old event is not read as a new one.
            Keyboard( int gamepad ) : io( gamepad )
            {
                int index = 0;

                caps_lock        = false;
                last_event_frame = -1;
                dropped_events   = 0;
                queue_first      = 0;
                queue_count      = 0;
                for( index = 0; index < KeyboardKeys; index++ )
                  held[ index ] = false;

                strobe_side = io.read() & KeyboardStrobe;
            }

            // Call once per frame: reads the device and queues the new key
            // event, if there is one. True when an event arrived. When the
            // queue is full (nobody reading) the event still updates
            // is_down() and caps lock, but is not queued.
            bool probe()
            {
                int raw  = 0;
                int side = 0;
                int key  = 0;
                int slot = 0;

                io.probe();
                raw  = io.read();
                side = raw & KeyboardStrobe;

                if( side == 0 || side == strobe_side )
                  return false;

                strobe_side      = side;
                last_event_frame = get_frame_counter();
                key = (raw >> KeyboardCodeShift) & KeyboardCodeMask;
                if( key == 0 )
                  return false;

                held[ key ] = (raw & KeyboardPressed) != 0;
                if( key == KeyCapsLock && held[ key ] )
                  caps_lock = !caps_lock;

                if( queue_count < KeyboardQueueSize )
                {
                    slot = (queue_first + queue_count) % KeyboardQueueSize;
                    queue_key[ slot ]     = key;
                    queue_symbol[ slot ]  = symbol( key );
                    queue_pressed[ slot ] = held[ key ];
                    queue_count++;
                }
                else
                  dropped_events++;

                return true;
            }

            // Next key PRESS (releases are skipped and discarded), as its
            // typed character -- shift and caps lock applied, compatible
            // with the BIOS font. Keys with no character return their key
            // code (always below 32, or 127). 0 when nothing is left.
            int read()
            {
                int result = 0;

                while( queue_count > 0 && result == 0 )
                {
                    if( queue_pressed[ queue_first ] )
                      result = queue_symbol[ queue_first ];
                    pop_event();
                }
                return result;
            }

            // Next event, press or release, as its KEY CODE (no shift
            // applied). If pressed is not NULL it is set to true for a
            // press, false for a release. 0 when nothing is left.
            int read_event( bool* pressed )
            {
                int result = 0;

                if( queue_count == 0 )
                  return 0;

                result = queue_key[ queue_first ];
                if( pressed != NULL )
                  *pressed = queue_pressed[ queue_first ];
                pop_event();
                return result;
            }

            // Next event with everything known about it. False (and *out
            // untouched) when nothing is left.
            bool next_event( KeyEvent* out )
            {
                if( queue_count == 0 )
                  return false;

                out->key     = queue_key[ queue_first ];
                out->symbol  = queue_symbol[ queue_first ];
                out->pressed = queue_pressed[ queue_first ];
                pop_event();
                return true;
            }

            // events waiting to be read
            int pending()
            {
                return queue_count;
            }

            // throw away every waiting event (held keys stay held)
            void flush()
            {
                queue_first = 0;
                queue_count = 0;
            }

            // Is the key held right now? Takes key codes: 'a', ' ',
            // v32::KeyLeft, v32::KeyLShift... For modifiers and for
            // game-style controls (WASD, arrows).
            bool is_down( int key )
            {
                if( key <= 0 || key >= KeyboardKeys )
                  return false;
                return held[ key ];
            }

            bool shift_down()
            {
                return held[ KeyLShift ] || held[ KeyRShift ];
            }

            bool ctrl_down()
            {
                return held[ KeyLCtrl ] || held[ KeyRCtrl ];
            }

            bool alt_down()
            {
                return held[ KeyLAlt ] || held[ KeyRAlt ];
            }

            bool gui_down()
            {
                return held[ KeyLGui ] || held[ KeyRGui ];
            }

            bool caps_lock_on()
            {
                return caps_lock;
            }

            // The typed character for a key code under the CURRENT shift
            // and caps lock state (US layout). Caps lock only affects
            // letters, and inverts shift there. Keys with no character
            // return the key code unchanged.
            int symbol( int key )
            {
                bool shift = held[ KeyLShift ] || held[ KeyRShift ];

                if( key >= 'a' && key <= 'z' )
                {
                    if( shift != caps_lock )
                      return key - 32;
                    return key;
                }

                if( !shift )
                  return key;

                switch( key )
                {
                    case '1':  return '!';
                    case '2':  return '@';
                    case '3':  return '#';
                    case '4':  return '$';
                    case '5':  return '%';
                    case '6':  return '^';
                    case '7':  return '&';
                    case '8':  return '*';
                    case '9':  return '(';
                    case '0':  return ')';
                    case '-':  return '_';
                    case '=':  return '+';
                    case '[':  return '{';
                    case ']':  return '}';
                    case '\\': return '|';
                    case ';':  return ':';
                    case '\'': return '"';
                    case ',':  return '<';
                    case '.':  return '>';
                    case '/':  return '?';
                    case '`':  return '~';
                }
                return key;
            }

            // frame of the latest key event, -1 before the first one
            int last_event()
            {
                return last_event_frame;
            }

            // events lost because the queue was full when they arrived
            int dropped()
            {
                return dropped_events;
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
