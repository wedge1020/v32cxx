// *****************************************************************************
//  tests/131sample.cpp — v32/keyboard.hpp: typed characters (extended
//  characters) sent by a v32io device, driven by scripted v32io input
//
//  A keyboard on gamepad port 1, fed by tests/131sample.input -- a v32peek
//  input script made from tests/131sample.actions by
//  tools/vircon32/v32io-script.py, which encodes characters the way the
//  v32io adaptor and the modified emulator send them: a codepage prefix
//  (key code 30 = BIOS characters 128-255, 31 = 0-127) and then a payload,
//  both as key releases. check.sh sees the .input file and runs this
//  cartridge with it.
//
//   phase 1  next_event(): characters come out as text events (key and
//            symbol = BIOS code, pressed); keys held meanwhile are
//            untouched; payloads equal to the prefix codes work; a prefix
//            that is never followed by a payload is forgotten after
//            KeyboardPageTimeout frames; a payload may be a key press
//   phase 2  read(): characters return their BIOS code, +KeyboardGlyph
//            below 32; glyph() recovers the code to draw
//   phase 3  read_event() skips characters (they are not keys)
//   phase 4  a prefix waiting when the gamepad is unplugged and plugged in
//            again is forgotten, and the strobe starts over
//
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

#include <v32/keyboard.hpp>

int test_errors = -1;

// phase 1: key, symbol, pressed, text -- in order
int expect_key[ 14 ]     = { 'a', 233, 158, 159, 30, 31, 'a', 65, 128, 255,
                             'x', 'x', 98, 'b' };
int expect_pressed[ 14 ] = { 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,
                             1, 0, 1, 0 };
int expect_text[ 14 ]    = { 0, 1, 1, 1, 1, 1, 0, 1, 1, 1,
                             0, 0, 1, 0 };

// phase 2: what read() must return, and the glyph() of each
int expect_read[ 7 ]     = { 233, 20 + v32::KeyboardGlyph, 31 + v32::KeyboardGlyph,
                             65, 200, v32::KeyEnter, v32::KeyF2 };
int expect_glyph[ 7 ]    = { 233, 20, 31, 65, 200, 0, 0 };

v32::Keyboard keyboard( v32::SecondGamepadPort );

int main()
{
    v32::KeyEvent event;
    int  e         = 0;
    int  phase     = 1;
    int  seen      = 0;
    int  reads     = 0;
    int  key       = 0;
    int  before_sel = 0;
    int  index     = 0;
    int  held_ok   = 0;
    int  quiet     = 0;
    int  z_presses = 0;
    int  z_text    = 0;
    bool pressed   = false;

    if( keyboard.port() != 1 ) e++;

    while( phase < 5 )
    {
        // probing selects the keyboard's gamepad to read it, and must put
        // the program's selection back (the console starts with the first
        // connected gamepad selected: here that is the keyboard's own)
        before_sel = get_selected_gamepad();
        keyboard.probe();
        if( get_selected_gamepad() != before_sel ) e++;

        if( phase == 1 )
        {
            while( phase == 1 && keyboard.next_event( &event ) )
            {
                if( event.key == v32::KeyF1 && !event.text )
                {
                    if( seen != 14 ) e++;
                    phase = 2;
                }
                else if( seen < 14 )
                {
                    if( event.key != expect_key[ seen ] ) e++;
                    if( event.symbol != expect_key[ seen ] ) e++;
                    if( (event.pressed ? 1 : 0) != expect_pressed[ seen ] ) e++;
                    if( (event.text ? 1 : 0) != expect_text[ seen ] ) e++;

                    // 'a' is held from its press to its release: the
                    // characters in between leave it alone
                    if( event.text && event.key == 233 && keyboard.is_down( 'a' ) )
                      held_ok++;
                    if( event.text && event.key == 31 && event.symbol == 31 && keyboard.is_down( 'a' ) )
                      held_ok++;
                    seen++;
                }
                else
                  e++;
            }
        }
        else if( phase == 2 )
        {
            key = keyboard.read();
            while( phase == 2 && key > 0 )
            {
                if( reads < 7 )
                {
                    if( key != expect_read[ reads ] ) e++;
                    if( keyboard.glyph( key ) != expect_glyph[ reads ] ) e++;
                    if( v32::key_glyph( key ) != expect_glyph[ reads ] ) e++;
                }
                reads++;
                if( key == v32::KeyF2 )
                  phase = 3;
                else
                  key = keyboard.read();
            }
        }
        else if( phase == 3 )
        {
            // F2's release comes first; then 233 (skipped), q down, q up,
            // 200 (skipped), F3 down
            key = keyboard.read_event( &pressed );
            while( phase == 3 && key > 0 )
            {
                if( key == 'q' )
                {
                    if( index == 0 && !pressed ) e++;
                    if( index == 1 && pressed ) e++;
                    index++;
                }
                else if( key == v32::KeyF3 && pressed )
                {
                    if( index != 2 ) e++;
                    phase = 4;
                }
                else if( key != v32::KeyF2 && key != v32::KeyF3 )
                  e++;

                if( phase == 3 )
                  key = keyboard.read_event( &pressed );
            }
        }
        else if( phase == 4 )
        {
            // the lone prefix is forgotten when the gamepad is plugged in
            // again: 'z' must come out as a key press and release, never as
            // a character (122 + 128 = 250)
            while( keyboard.next_event( &event ) )
            {
                if( event.key == 'z' && event.pressed && !event.text ) z_presses++;
                if( event.text ) z_text++;
            }
            if( z_presses > 0 )
              quiet++;
            if( quiet > 30 )
              phase = 5;
        }

        end_frame();
    }

    if( reads != 7 ) e++;
    if( held_ok != 2 ) e++;
    if( z_presses != 1 || z_text != 0 ) e++;
    if( keyboard.is_down( 'a' ) ) e++;
    if( keyboard.dropped() != 0 ) e++;

    // the glyph() function on its own: characters, key codes, and nonsense
    if( v32::key_glyph( 32 ) != 32 || v32::key_glyph( 126 ) != 126 ) e++;
    if( v32::key_glyph( 128 ) != 128 || v32::key_glyph( 255 ) != 255 ) e++;
    if( v32::key_glyph( 14 + v32::KeyboardGlyph ) != 14 ) e++;
    if( v32::key_glyph( 1 + v32::KeyboardGlyph ) != 1 ) e++;
    if( v32::key_glyph( 31 + v32::KeyboardGlyph ) != 31 ) e++;
    if( v32::key_glyph( v32::KeyEnter ) != 0 || v32::key_glyph( v32::KeyF12 ) != 0 ) e++;
    if( v32::key_glyph( v32::KeyEscape ) != 0 || v32::key_glyph( v32::KeyDelete ) != 0 ) e++;
    if( v32::key_glyph( 0 ) != 0 || v32::key_glyph( -1 ) != 0 ) e++;
    if( v32::key_glyph( v32::KeyboardGlyph ) != 0 ) e++;
    if( v32::key_glyph( v32::KeyboardGlyph + 32 ) != 0 || v32::key_glyph( 300 ) != 0 ) e++;
    if( v32::KeyPageHigh != 30 || v32::KeyPageLow != 31 ) e++;

    test_errors = e;
    return 0;
}
