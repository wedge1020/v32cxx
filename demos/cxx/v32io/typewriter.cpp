// *****************************************************************************
//  demos/cxx/v32io/typewriter.cpp — type text on the screen with a v32io
//  keyboard (the C++ counterpart of the v32io library's v32kbd.c demo)
//
//  NEEDS a v32io keyboard on GAMEPAD 2: the v32io adaptor with a USB
//  keyboard in it (joystick `v32io:kbd`), or the modified DesktopEmulator
//  with Gamepads > Gamepad 2 > v32kbd. The stock emulator's "Keyboard"
//  device will NOT work. See docs/V32IO.md.
//
//      typing        shift and caps lock apply, as on a US layout
//      Enter / Tab   new line / 4 spaces
//      Backspace     delete the last character
//      Escape        clear the page
//
//  The top line shows the last key's code and which modifiers are held.
// *****************************************************************************

#title   "v32io typewriter"
#version 1.0

#include "video.h"
#include "string.h"
#include <v32/v32io.hpp>

#define KEYBOARD_PORT  v32::SecondGamepadPort   // Gamepad 2
#define COLUMNS        64                       // 640 / 10-pixel glyphs
#define ROWS           15                       // lines of text on screen
#define TEXT_MAX       (COLUMNS * ROWS)

// -----------------------------------------------------------------------------
//  Page — the typed text, wrapped at COLUMNS characters
// -----------------------------------------------------------------------------

class Page
{
    int text[ TEXT_MAX + 1 ];
    int length;

    public:
        Page()
        {
            clear();
        }

        void clear()
        {
            length    = 0;
            text[ 0 ] = 0;
        }

        // lines the text takes, counting wrapped ones
        int lines()
        {
            int count  = 1;
            int column = 0;
            for( int i = 0; i < length; i++ )
            {
                if( text[ i ] == '\n' || column == COLUMNS )
                {
                    count++;
                    column = 0;
                }
                if( text[ i ] != '\n' )
                  column++;
            }
            return count;
        }

        void add( int character )
        {
            if( length >= TEXT_MAX )
              return;
            text[ length ] = character;
            length++;
            text[ length ] = 0;
            if( lines() > ROWS )           // full page: take it back
              backspace();
        }

        void backspace()
        {
            if( length > 0 )
            {
                length--;
                text[ length ] = 0;
            }
        }

        // draw at (x, y), wrapping, with a cursor after the last character
        void draw( int x, int y, bool cursor )
        {
            int line[ COLUMNS + 2 ];
            int used = 0;
            int row  = 0;

            for( int i = 0; i <= length; i++ )
            {
                bool end = (i == length);
                if( end || text[ i ] == '\n' || used == COLUMNS )
                {
                    if( end && cursor && used < COLUMNS )
                    {
                        line[ used ] = '_';
                        used++;
                    }
                    line[ used ] = 0;
                    print_at( x, y + row * 20, line );
                    row++;
                    used = 0;
                    if( !end && text[ i ] == '\n' )
                      continue;
                }
                if( !end )
                {
                    line[ used ] = text[ i ];
                    used++;
                }
            }
        }
};

v32::Keyboard keyboard( KEYBOARD_PORT );
Page page;

void main()
{
    int last = 0;
    int key  = 0;
    int code[ 12 ];

    while( true )
    {
        // once on every frame, or key events get lost
        keyboard.probe();

        key = keyboard.read();
        while( key > 0 )
        {
            last = key;
            if( key == v32::KeyBackspace )
              page.backspace();
            else if( key == v32::KeyEscape )
              page.clear();
            else if( key == v32::KeyEnter )
              page.add( '\n' );
            else if( key == v32::KeyTab )
            {
                for( int i = 0; i < 4; i++ )
                  page.add( ' ' );
            }
            else if( key >= 32 && key < 127 )
              page.add( key );

            key = keyboard.read();
        }

        clear_screen( color_black );

        set_multiply_color( color_gray );
        print_at( 0, 0, "KEY:" );
        itoa( last, code, 10 );
        set_multiply_color( color_white );
        print_at( 50, 0, code );

        // light up each modifier while it is held
        set_multiply_color( keyboard.shift_down()   ? color_yellow : color_darkgray );
        print_at( 100, 0, "SHIFT" );
        set_multiply_color( keyboard.ctrl_down()    ? color_yellow : color_darkgray );
        print_at( 170, 0, "CTRL" );
        set_multiply_color( keyboard.alt_down()     ? color_yellow : color_darkgray );
        print_at( 230, 0, "ALT" );
        set_multiply_color( keyboard.gui_down()     ? color_yellow : color_darkgray );
        print_at( 280, 0, "GUI" );
        set_multiply_color( keyboard.caps_lock_on() ? color_green  : color_darkgray );
        print_at( 330, 0, "CAPS" );

        set_multiply_color( color_white );
        if( !keyboard.connected() )
        {
            set_multiply_color( color_red );
            print_at( 0, 340, "No device on Gamepad 2 -- see docs/V32IO.md" );
            set_multiply_color( color_white );
        }

        // blink the cursor every half second
        page.draw( 0, 40, (get_frame_counter() / 30) % 2 == 0 );
        end_frame();
    }
}
