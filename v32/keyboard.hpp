#pragma once
// *****************************************************************************
//  v32/keyboard.hpp — the v32kbd keyboard driver, in C++
//
//  v32::Keyboard (the v32io C library's keyboard.h), built on the v32io core in
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
//  EXTENDED CHARACTERS: key codes identify keys, so they cannot say what a
//  key typed when that is not ASCII (an accented letter from a dead key or
//  AltGr, a box drawing character...). A v32io device can send such a
//  TYPED CHARACTER instead -- in the character set of the BIOS font
//  (Windows-1252, with box drawing characters in place of codes 14 to 31)
//  -- as two consecutive events: a codepage PREFIX (KeyPageHigh or
//  KeyPageLow, two key codes that used to be unused) and then a PAYLOAD
//  with the low 7 bits of the character:
//
//      KeyPageHigh + payload  ->  character 128 + payload  (128 to 255)
//      KeyPageLow  + payload  ->  character payload        (0 to 127)
//
//  probe() decodes them; read() returns them like any typed character
//  (their BIOS font code, plus KeyboardGlyph below 32, where a character
//  would be mistaken for a key code), and glyph() turns any value read()
//  returned into the code to draw. See docs/V32IO.md, "Typed characters".
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
        KeyPageHigh  = 30,    // codepage prefix: BIOS characters 128-255
        KeyPageLow   = 31,    // codepage prefix: BIOS characters 0-127
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
        KeyboardQueueSize = 64,    // events that can wait to be read
        KeyboardGlyph     = 256,   // added by read() to typed characters
                                   // below 32 (box drawing...): see glyph()
        KeyboardPageTimeout = 30   // frames a codepage prefix waits for its
                                   // payload
    };

    // -------------------------------------------------------------------------
    //  key_glyph — from a value returned by Keyboard::read(), the BIOS font
    //  character to draw (the code print_at() takes), or 0 when the value is
    //  not a typed character: a key with no character (arrows, function keys,
    //  escape, delete...) or no key at all. It tells a key code below 32
    //  apart from a box drawing character that has the same code.
    // -------------------------------------------------------------------------

    int key_glyph( int value )
    {
        if( (value >= 32 && value < 127) || (value >= 128 && value < 256) )
          return value;

        if( value > KeyboardGlyph && value < KeyboardGlyph + 32 )
          return value - KeyboardGlyph;

        return 0;
    }

    // one key event, as next_event() hands it out
    struct KeyEvent
    {
        int  key;        // key code (a character, or a Key constant); for a
                         // typed character (text), its BIOS font code
        int  symbol;     // typed character: shift and caps lock applied as
                         // they were when the key was pressed (same as key
                         // for keys with no character, and for text)
        bool pressed;    // true: pressed, false: released (always true for
                         // a typed character)
        bool text;       // true: a typed character sent by the device
                         // (extended characters), not a key
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
    //  one style per keyboard. is_down() works alongside either. Typed
    //  characters sent by the device (see the top of this file) come out of
    //  read() and next_event(); read_event() skips them, as they are not
    //  keys.
    // -------------------------------------------------------------------------

    class Keyboard
    {
        IoDevice io;
        int  strobe_side;        // last strobe side seen (0 none, 1 L, 2 R)
        int  page;               // codepage prefix waiting for its payload
                                 // (0 none, else KeyPageHigh or KeyPageLow)
        int  page_frame;         // frame at which that prefix was received
        bool caps_lock;          // caps lock state (toggles on each press)
        int  last_event_frame;   // frame of the latest key event (-1: none)
        int  dropped_events;     // events lost to a full queue
        int  queue_first;        // ring buffer: index of the oldest event
        int  queue_count;        // ring buffer: events waiting
        int  queue_key[ KeyboardQueueSize ];
        int  queue_symbol[ KeyboardQueueSize ];
        bool queue_pressed[ KeyboardQueueSize ];
        bool queue_text[ KeyboardQueueSize ];
        bool held[ KeyboardKeys ];

        // drop the oldest queued event (caller checks queue_count > 0)
        void pop_event()
        {
            queue_first = (queue_first + 1) % KeyboardQueueSize;
            queue_count--;
        }

        // add an event, unless nobody is reading them (queue full)
        void enqueue( int key, int typed, bool pressed, bool text )
        {
            int slot = 0;

            if( queue_count < KeyboardQueueSize )
            {
                slot = (queue_first + queue_count) % KeyboardQueueSize;
                queue_key[ slot ]     = key;
                queue_symbol[ slot ]  = typed;
                queue_pressed[ slot ] = pressed;
                queue_text[ slot ]    = text;
                queue_count++;
            }
            else
              dropped_events++;
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
                page             = 0;
                page_frame       = 0;
                dropped_events   = 0;
                queue_first      = 0;
                queue_count      = 0;
                for( index = 0; index < KeyboardKeys; index++ )
                  held[ index ] = false;

                strobe_side = io.read() & KeyboardStrobe;
            }

            // Call once per frame: reads the device and queues the new key
            // event, if there is one. True when an event arrived (a key
            // event or a complete typed character; a codepage prefix alone
            // is not one). When the queue is full (nobody reading) the
            // event still updates is_down() and caps lock, but is not
            // queued.
            bool probe()
            {
                bool result    = false;
                bool handled   = false;
                int  raw       = 0;
                int  side      = 0;
                int  key       = 0;
                int  frame     = 0;
                int  character = 0;

                io.probe();
                raw  = io.read();

                // A gamepad that has just been connected shows every control
                // released, so the history of the device is lost: the strobe
                // starts over (the first event will be a new one, whatever
                // side it is) and a prefix that was waiting won't get its
                // payload.
                if( io.plugged() )
                {
                    strobe_side = 0;
                    page        = 0;
                }

                side = raw & KeyboardStrobe;
                if( side == 0 || side == strobe_side )
                  return false;

                strobe_side      = side;
                frame            = get_frame_counter();
                last_event_frame = frame;
                key = (raw >> KeyboardCodeShift) & KeyboardCodeMask;

                // A prefix is waiting: this event is its payload (any code,
                // including 0 and the prefix codes themselves, and whatever
                // the action). A prefix that has waited too long was lost
                // with its payload: forget it and read this event as a
                // regular one.
                if( page != 0 )
                {
                    if( frame - page_frame <= KeyboardPageTimeout )
                    {
                        handled   = true;
                        result    = true;
                        character = key;
                        if( page == KeyPageHigh )
                          character = key + 128;
                        page = 0;

                        // character 0 is nothing
                        if( character > 0 )
                          enqueue( character, character, true, true );
                    }
                    else
                      page = 0;
                }

                // Prefixes carry no key: they announce that the next event
                // is a character.
                if( !handled && (key == KeyPageHigh || key == KeyPageLow) )
                {
                    handled    = true;
                    page       = key;
                    page_frame = frame;
                }

                if( !handled && key > 0 )
                {
                    result = true;

                    held[ key ] = (raw & KeyboardPressed) != 0;
                    if( key == KeyCapsLock && held[ key ] )
                      caps_lock = !caps_lock;

                    enqueue( key, symbol( key ), held[ key ], false );
                }

                return result;
            }

            // Next key PRESS (releases are skipped and discarded), as its
            // typed character -- shift and caps lock applied, compatible
            // with the BIOS font. Keys with no character return their key
            // code (always below 32, or 127). Typed characters sent by the
            // device (accented letters, box drawing...) are returned too:
            // as their BIOS font code, or for codes below 32 (which are
            // also key codes) that code plus KeyboardGlyph. Pass any value
            // read() returns to glyph() to get the code to draw. 0 when
            // nothing is left.
            int read()
            {
                int result = 0;

                while( queue_count > 0 && result == 0 )
                {
                    if( queue_pressed[ queue_first ] )
                    {
                        result = queue_symbol[ queue_first ];
                        if( queue_text[ queue_first ] && result < 32 )
                          result += KeyboardGlyph;
                    }
                    pop_event();
                }
                return result;
            }

            // The BIOS font character to draw for a value read() returned
            // (0 for a key with no character). See key_glyph().
            int glyph( int value )
            {
                return key_glyph( value );
            }

            // Next event, press or release, as its KEY CODE (no shift
            // applied). If pressed is not NULL it is set to true for a
            // press, false for a release. Typed characters sent by the
            // device are not keys, so they are skipped (and discarded):
            // read them with read() or next_event(). 0 when nothing is
            // left.
            int read_event( bool* pressed )
            {
                int result = 0;

                while( queue_count > 0 && queue_text[ queue_first ] )
                  pop_event();

                if( queue_count == 0 )
                  return 0;

                result = queue_key[ queue_first ];
                if( pressed != NULL )
                  *pressed = queue_pressed[ queue_first ];
                pop_event();
                return result;
            }

            // Next event with everything known about it -- keys and typed
            // characters alike (see KeyEvent::text). False (and *out
            // untouched) when nothing is left.
            bool next_event( KeyEvent* out )
            {
                if( queue_count == 0 )
                  return false;

                out->key     = queue_key[ queue_first ];
                out->symbol  = queue_symbol[ queue_first ];
                out->pressed = queue_pressed[ queue_first ];
                out->text    = queue_text[ queue_first ];
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
}
