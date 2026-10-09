// *****************************************************************************
//  demos/cxx/v32io/paint.cpp — draw on the screen with a v32io mouse (the
//  C++ counterpart of the v32io library's v32mouse.c demo)
//
//  NEEDS a v32io mouse on GAMEPAD 2: the v32io adaptor with a USB mouse in
//  it (joystick `v32io:mouse`), or the modified DesktopEmulator with
//  Gamepads > Gamepad 2 > v32mouse (the emulator captures the pointer when
//  its window is focused: left Ctrl + left Alt releases it). The stock
//  emulator has no mouse device at all. See docs/V32IO.md.
//
//      left button     hold to draw
//      right button    clear the drawing
//      middle button   next colour
//
//  The top line shows the pointer, the latest movement and the buttons.
// *****************************************************************************

#title   "v32io paint"
#version 1.0

#include "video.h"
#include "string.h"
#include <v32/math.hpp>
#include <v32/v32io.hpp>

#define MOUSE_PORT  v32::SecondGamepadPort   // Gamepad 2
#define DOTS_MAX    1200
#define DOT_SPACING 4                       // pixels between dots in a stroke
#define COLORS      5

// -----------------------------------------------------------------------------
//  Drawing — the dots painted so far
// -----------------------------------------------------------------------------

class Drawing
{
    int dot_x[ DOTS_MAX ];
    int dot_y[ DOTS_MAX ];
    int dot_color[ DOTS_MAX ];
    int dots;

    public:
        Drawing()
        {
            dots = 0;
        }

        void clear()
        {
            dots = 0;
        }

        bool full()
        {
            return dots == DOTS_MAX;
        }

        // a dot, unless the last one is already there in that colour
        void add( int x, int y, int color )
        {
            if( full() )
              return;
            if( dots > 0 && dot_x[ dots - 1 ] == x && dot_y[ dots - 1 ] == y &&
                dot_color[ dots - 1 ] == color )
              return;
            dot_x[ dots ]     = x;
            dot_y[ dots ]     = y;
            dot_color[ dots ] = color;
            dots++;
        }

        // dots every DOT_SPACING pixels from (x0, y0) to (x1, y1); the
        // first point is already there from the previous frame
        void line_to( int x0, int y0, int x1, int y1, int color )
        {
            int dx    = x1 - x0;
            int dy    = y1 - y0;
            int steps = v32::absolute( dx );
            if( v32::absolute( dy ) > steps )
              steps = v32::absolute( dy );
            steps = steps / DOT_SPACING;
            if( steps < 1 )
              steps = 1;
            for( int i = 1; i <= steps; i++ )
              add( x0 + dx * i / steps, y0 + dy * i / steps, color );
        }

        void draw()
        {
            for( int i = 0; i < dots; i++ )
            {
                set_multiply_color( dot_color[ i ] );
                print_at( dot_x[ i ] - 5, dot_y[ i ] - 10, "*" );
            }
            set_multiply_color( color_white );
        }

        int count()
        {
            return dots;
        }
};

int palette[ COLORS ] = { color_white, color_red, color_green, color_yellow, color_cyan };

v32::Mouse mouse( MOUSE_PORT );
Drawing drawing;

// print a number at (x, y)
void print_number( int x, int y, int value )
{
    int digits[ 12 ];
    itoa( value, digits, 10 );
    print_at( x, y, digits );
}

void main()
{
    int color   = 0;
    int shown_dx = 0;
    int shown_dy = 0;
    int last_x   = 0;
    int last_y   = 0;

    // keep the pointer clear of the status line
    mouse.set_bounds( 0, 30, screen_width - 1, screen_height - 1 );

    while( true )
    {
        // once on every frame, or movement gets misread
        mouse.probe();

        // keep the last non-zero movement on screen long enough to read
        if( mouse.dx() != 0 || mouse.dy() != 0 )
          mouse.delta( &shown_dx, &shown_dy );

        if( mouse.pressed( v32::MouseRight ) )
          drawing.clear();
        if( mouse.pressed( v32::MouseMiddle ) )
          color = (color + 1) % COLORS;
        // a fast stroke moves the pointer up to 10 pixels per frame:
        // fill in the segment since the last frame so lines have no gaps
        if( mouse.is_down( v32::MouseLeft ) )
        {
            if( mouse.pressed( v32::MouseLeft ) )
              drawing.add( mouse.x(), mouse.y(), palette[ color ] );
            else
              drawing.line_to( last_x, last_y, mouse.x(), mouse.y(), palette[ color ] );
        }
        last_x = mouse.x();
        last_y = mouse.y();

        clear_screen( color_black );
        drawing.draw();

        print_at( 0, 0, "X:     Y:     DX:    DY:" );
        print_number(  30, 0, mouse.x() );
        print_number( 100, 0, mouse.y() );
        print_number( 180, 0, shown_dx );
        print_number( 250, 0, shown_dy );

        print_at( 310, 0, "[L] [M] [R]" );
        if( mouse.is_down( v32::MouseLeft ) )   print_at( 320, 0, "*" );
        if( mouse.is_down( v32::MouseMiddle ) ) print_at( 360, 0, "*" );
        if( mouse.is_down( v32::MouseRight ) )  print_at( 400, 0, "*" );

        set_multiply_color( palette[ color ] );
        print_at( 440, 0, "COLOUR" );
        set_multiply_color( drawing.full() ? color_red : color_gray );
        print_number( 520, 0, drawing.count() );

        if( !mouse.connected() )
        {
            set_multiply_color( color_red );
            print_at( 0, 340, "No device on Gamepad 2 -- see docs/V32IO.md" );
        }

        set_multiply_color( color_cyan );
        print_at( mouse.x() - 5, mouse.y() - 10, "+" );
        set_multiply_color( color_white );

        end_frame();
    }
}
