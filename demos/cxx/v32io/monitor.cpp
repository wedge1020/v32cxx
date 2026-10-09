// *****************************************************************************
//  demos/cxx/v32io/monitor.cpp — what is each gamepad port sending?
//
//  A setup and troubleshooting tool. For all four gamepad ports it shows
//  the 11 controls exactly as the console sees them (the packed controls of
//  v32::IoDevice), and what they would mean to each v32io driver:
//
//      KBD    the strobe side, press/release, and the 7-bit key code
//      MOUSE  the two movement-counter positions (0-11) and the buttons
//
//  Use it to check a new setup before blaming a program:
//   - the adaptor's joystick profile must map button n to control n (see
//     docs/V32IO.md). Press a key: the key code bits should change, and
//     only Left/Right should alternate, one per key event. A control that
//     never lights up, or two that always move together, is a mapping
//     mistake in the profile;
//   - with a mouse, moving right should walk "CX" up one position at a
//     time (0..11, wrapping), moving down walks "CY" up, and a counter
//     showing "--" means an impossible state (Left and Right both on);
//   - an ordinary gamepad shows up too: handy to tell which port is which.
//
//  It works with no v32io device at all (then it is just a gamepad viewer),
//  but needs the v32io adaptor or the modified DesktopEmulator to show v32io
//  traffic. See docs/V32IO.md.
// *****************************************************************************

#title   "v32io monitor"
#version 1.0

#include "video.h"
#include "string.h"
#include <v32/v32io.hpp>      // v32::IoDevice
#include <v32/keyboard.hpp>   // the keyboard protocol constants
#include <v32/mouse.hpp>      // v32::mouse_counter

#define PORTS 4

// control labels, in packed-control bit order
int labels[ 11 ] = { 'L', 'R', 'U', 'D', 'S', 'A', 'B', 'X', 'Y', 'l', 'r' };

v32::IoDevice* devices[ PORTS ];
int events[ PORTS ];                   // packed-control changes seen

void print_number( int x, int y, int value )
{
    int digits[ 12 ];
    itoa( value, digits, 10 );
    print_at( x, y, digits );
}

// one port's block of the screen, starting at row y
void show_port( int index, int y )
{
    v32::IoDevice* io = devices[ index ];
    int raw     = io->read();
    int changed = io->changed();
    int glyph[ 2 ];
    int side    = 0;
    int counter = 0;

    set_multiply_color( io->connected() ? color_white : color_darkgray );
    print_at( 0, y, "GAMEPAD" );
    print_number( 80, y, index + 1 );
    print_at( 110, y, io->connected() ? "connected" : "not connected" );
    print_at( 300, y, "raw" );
    print_number( 340, y, raw );
    print_at( 420, y, "changes" );
    print_number( 500, y, events[ index ] );

    // the 11 controls: lit when on, flashing yellow on the frame they change
    glyph[ 1 ] = 0;
    for( int bit = 0; bit < v32::IoControls; bit++ )
    {
        int mask = 1 << bit;
        if( (changed & mask) != 0 )
          set_multiply_color( color_yellow );
        else if( (raw & mask) != 0 )
          set_multiply_color( color_green );
        else
          set_multiply_color( color_darkgray );
        glyph[ 0 ] = labels[ bit ];
        print_at( 20 + bit * 20, y + 22, glyph );
    }

    // as a keyboard
    set_multiply_color( color_lightgray );
    print_at( 260, y + 22, "KBD" );
    side = raw & v32::KeyboardStrobe;
    print_at( 300, y + 22, side == 1 ? "L" : (side == 2 ? "R" : "-") );
    print_at( 320, y + 22, (raw & v32::KeyboardPressed) != 0 ? "dn" :
                           ((raw & v32::KeyboardReleased) != 0 ? "up" : "--") );
    print_number( 350, y + 22, (raw >> v32::KeyboardCodeShift) & v32::KeyboardCodeMask );

    // as a mouse
    print_at( 410, y + 22, "MOUSE CX" );
    counter = v32::mouse_counter( raw, v32::MouseXNegative, v32::MouseXPositive,
                                       v32::MouseXHigh, v32::MouseXLow );
    if( counter < 0 ) print_at( 500, y + 22, "--" );
    else              print_number( 500, y + 22, counter );
    print_at( 530, y + 22, "CY" );
    counter = v32::mouse_counter( raw, v32::MouseYNegative, v32::MouseYPositive,
                                       v32::MouseYHigh, v32::MouseYLow );
    if( counter < 0 ) print_at( 560, y + 22, "--" );
    else              print_number( 560, y + 22, counter );
    print_at( 590, y + 22, (raw & v32::MouseButtonLeft) != 0 ? "L" : "." );
    print_at( 605, y + 22, (raw & v32::MouseButtonMiddle) != 0 ? "M" : "." );
    print_at( 620, y + 22, (raw & v32::MouseButtonRight) != 0 ? "R" : "." );
}

void main()
{
    for( int i = 0; i < PORTS; i++ )
    {
        devices[ i ] = new v32::IoDevice( i );
        events[ i ]  = 0;
    }

    while( true )
    {
        for( int i = 0; i < PORTS; i++ )
          if( devices[ i ]->probe() )
            events[ i ]++;

        clear_screen( color_black );
        set_multiply_color( color_cyan );
        print_at( 0, 0, "v32io monitor: the 11 controls of each gamepad port" );
        for( int i = 0; i < PORTS; i++ )
          show_port( i, 40 + i * 75 );
        set_multiply_color( color_white );

        end_frame();
    }
}
