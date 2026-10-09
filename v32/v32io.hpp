#pragma once
// *****************************************************************************
//  v32/v32io.hpp — the v32io core, in C++
//
//  A C++ port of the v32io Vircon32 C drivers
//  (https://github.com/wedge1020/v32io, lib/), for v32c++ programs, split
//  the same way the C library is:
//
//      v32/v32io.hpp      v32::IoDevice: a gamepad's 11 controls read as
//                         data (the C library's v32io.h)
//      v32/keyboard.hpp   v32::Keyboard: a v32kbd keyboard (keyboard.h)
//      v32/mouse.hpp      v32::Mouse: a v32mouse mouse (mouse.h)
//
//  A program includes the driver(s) it needs; each includes this core.
//  Keyboard and mouse can be used at the same time, each on its own
//  gamepad port. Same protocols, same decoding, same once-per-frame rules
//  as the C library.
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
//  the generated C. Unlike the other v32/ headers these wrap no SDK
//  header of their own: the v32io C library is NOT needed to build a
//  v32c++ program that uses them (everything is here), and the two are
//  not meant to be mixed in one program.
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
}
