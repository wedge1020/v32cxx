// *****************************************************************************
//  demos/cxx/v32io/notepad.cpp — a keyboard AND a mouse at the same time
//
//  A page of 64 x 15 character cells. Click a cell to put the cursor there
//  and type; each device has its own gamepad port and its own driver
//  object, and both are probed once per frame.
//
//  NEEDS a v32io keyboard on GAMEPAD 2 and a v32io mouse on GAMEPAD 3:
//  two v32io adaptors (one with a keyboard, one with a mouse), or the
//  modified DesktopEmulator with Gamepad 2 > v32kbd and Gamepad 3 >
//  v32mouse. Gamepad 1 stays an ordinary gamepad. See docs/V32IO.md.
//
//      left click            move the cursor to that cell
//      right click           erase that cell
//      typing                writes at the cursor and moves it on
//      arrow keys            move the cursor
//      Backspace / Delete    erase before / at the cursor
//      Enter                 start of the next line
//      Ctrl + Escape         erase the whole page
// *****************************************************************************

#title   "v32io notepad"
#version 1.0

#include "video.h"
#include <v32/v32io.hpp>

#define KEYBOARD_PORT  v32::SecondGamepadPort   // Gamepad 2
#define MOUSE_PORT     v32::ThirdGamepadPort    // Gamepad 3
#define COLUMNS        64
#define ROWS           15
#define TOP            40                       // first row's y, in pixels
#define CELL_W         10                       // BIOS font glyph size
#define CELL_H         20

// -----------------------------------------------------------------------------
//  Grid — the page, with a cursor
// -----------------------------------------------------------------------------

class Grid
{
    int cells[ ROWS ][ COLUMNS ];
    int cursor_column;
    int cursor_row;

    public:
        Grid()
        {
            clear();
        }

        void clear()
        {
            for( int r = 0; r < ROWS; r++ )
              for( int c = 0; c < COLUMNS; c++ )
                cells[ r ][ c ] = ' ';
            cursor_column = 0;
            cursor_row    = 0;
        }

        void move_to( int column, int row )
        {
            if( column < 0 ) column = 0;
            if( column >= COLUMNS ) column = COLUMNS - 1;
            if( row < 0 ) row = 0;
            if( row >= ROWS ) row = ROWS - 1;
            cursor_column = column;
            cursor_row    = row;
        }

        void move_by( int columns, int rows )
        {
            move_to( cursor_column + columns, cursor_row + rows );
        }

        void put( int character )
        {
            cells[ cursor_row ][ cursor_column ] = character;
            if( cursor_column < COLUMNS - 1 )
              cursor_column++;
            else if( cursor_row < ROWS - 1 )
              move_to( 0, cursor_row + 1 );
        }

        void erase_at( int column, int row )
        {
            if( column >= 0 && column < COLUMNS && row >= 0 && row < ROWS )
              cells[ row ][ column ] = ' ';
        }

        void backspace()
        {
            if( cursor_column > 0 )
              cursor_column--;
            else if( cursor_row > 0 )
              move_to( COLUMNS - 1, cursor_row - 1 );
            erase_at( cursor_column, cursor_row );
        }

        void delete_here()
        {
            erase_at( cursor_column, cursor_row );
        }

        void new_line()
        {
            move_to( 0, cursor_row + 1 );
        }

        void draw( bool show_cursor )
        {
            int line[ COLUMNS + 1 ];
            set_multiply_color( color_white );
            for( int r = 0; r < ROWS; r++ )
            {
                for( int c = 0; c < COLUMNS; c++ )
                  line[ c ] = cells[ r ][ c ];
                line[ COLUMNS ] = 0;
                print_at( 0, TOP + r * CELL_H, line );
            }
            if( show_cursor )
            {
                set_multiply_color( color_yellow );
                print_at( cursor_column * CELL_W, TOP + cursor_row * CELL_H, "_" );
                set_multiply_color( color_white );
            }
        }
};

v32::Keyboard keyboard( KEYBOARD_PORT );
v32::Mouse    mouse( MOUSE_PORT );
Grid          grid;

// the cell under the mouse pointer
int pointer_column()
{
    return mouse.x() / CELL_W;
}

int pointer_row()
{
    return (mouse.y() - TOP) / CELL_H;
}

void handle_key( int key )
{
    if( key == v32::KeyEscape && keyboard.ctrl_down() )
      grid.clear();
    else if( key == v32::KeyLeft )      grid.move_by( -1, 0 );
    else if( key == v32::KeyRight )     grid.move_by( 1, 0 );
    else if( key == v32::KeyUp )        grid.move_by( 0, -1 );
    else if( key == v32::KeyDown )      grid.move_by( 0, 1 );
    else if( key == v32::KeyBackspace ) grid.backspace();
    else if( key == v32::KeyDelete )    grid.delete_here();
    else if( key == v32::KeyEnter )     grid.new_line();
    else if( key >= 32 && key < 127 && !keyboard.ctrl_down() )
      grid.put( key );
}

void main()
{
    int key = 0;

    // the pointer moves only over the page, one pixel per device step
    mouse.set_bounds( 0, TOP, COLUMNS * CELL_W - 1, TOP + ROWS * CELL_H - 1 );
    mouse.set_scale( 1 );

    while( true )
    {
        // every device, once on every frame
        keyboard.probe();
        mouse.probe();

        key = keyboard.read();
        while( key > 0 )
        {
            handle_key( key );
            key = keyboard.read();
        }

        if( mouse.pressed( v32::MouseLeft ) )
          grid.move_to( pointer_column(), pointer_row() );
        if( mouse.is_down( v32::MouseRight ) )
          grid.erase_at( pointer_column(), pointer_row() );

        clear_screen( color_black );
        set_multiply_color( color_cyan );
        print_at( 0, 0, "v32io notepad -- keyboard on Gamepad 2, mouse on Gamepad 3" );
        if( !keyboard.connected() || !mouse.connected() )
        {
            set_multiply_color( color_red );
            print_at( 0, 340, "Missing device(s) -- see docs/V32IO.md" );
        }

        grid.draw( (get_frame_counter() / 30) % 2 == 0 );

        // the pointer: a block over the cell it is on
        set_multiply_color( 0x80FFFF00 );
        print_at( pointer_column() * CELL_W, TOP + pointer_row() * CELL_H, "#" );
        set_multiply_color( color_white );

        end_frame();
    }
}
