// *****************************************************************************
//  demos/cxx/v32io/glyphs.cpp — the BIOS font's characters, and typing them
//  with a v32io keyboard
//
//  The Vircon32 BIOS font has more than ASCII: the Windows-1252 letters and
//  symbols (é ñ ß © € « » ...) and, in place of codes 14 to 31, box drawing
//  characters and shades (┌─┐ │ └┘ ░▒▓█). A v32io keyboard can type all of
//  them: the modified emulator hands over whatever the computer's own layout,
//  dead keys and compose keys type, and the adaptor has AltGr combinations,
//  dead keys and Alt codes (docs/V32IO.md, "Typed characters").
//
//  Three pages, shown in turn (or pick one):
//
//    MAP      every character the font can draw, by code
//    COMPOSE  how to type them, and a box to try it in: it also tells you
//             whether the last thing typed was a KEY or a typed CHARACTER
//    SCENE    a window, languages, symbols and shades -- all font glyphs
//
//  The MAP and SCENE pages need no device at all. COMPOSE needs a v32io
//  keyboard on GAMEPAD 3 (the adaptor, or the modified DesktopEmulator with
//  Gamepads > Gamepad 3 > v32kbd). The stock "Keyboard" device won't work.
//
//      F1 F2 F3        pick a page          Tab, Right, Left   next / previous
//      gamepad 1:      Left / Right (or L / R) previous / next
//      Backspace       delete, on COMPOSE   Enter              clear the box
//
//  With no input for a while the pages turn by themselves.
//
//  Characters are written with octal  escapes ("\351" is é) because the
//  BIOS font codes are  not Unicode; the text each one  spells is in the
//  comment  beside it.  Codes 14  to 31  (the box  drawing set)  work in
//  string  literals too,  "\031\025\030" being  ┌─┐: the  Vircon32
//  toolchain  can't carry  those  in  a literal,  so  v32c++ moves  such
//  literals into hidden global arrays (see docs/VIRCON32_QUIRKS.md).
//
// *****************************************************************************

#title   "v32io glyphs"
#version 1.0

#include "video.h"
#include "input.h"
#include "string.h"
#include <v32/keyboard.hpp>

#define KEYBOARD_PORT  v32::ThirdGamepadPort   // Gamepad 3
#define PAGES          3
#define AUTO_FRAMES    480                      // idle frames before the page turns
#define TYPED_COLUMNS  58                       // the box on the COMPOSE page
#define TYPED_ROWS     3
#define TYPED_MAX      (TYPED_COLUMNS * TYPED_ROWS)

// BIOS font codes of the box drawing set
#define SHADE_1        17    // light shade
#define SHADE_2        18
#define SHADE_3        19
#define SOLID          20    // full block
#define BOX_H          21    // horizontal line
#define BOX_V          22    // vertical line
#define BOX_BR         23    // corners: bottom right
#define BOX_TR         24    // top right
#define BOX_TL         25    // top left
#define BOX_BL         26    // bottom left

v32::Keyboard keyboard( KEYBOARD_PORT );

int page        = 0;
int idle        = 0;
int typed[ TYPED_MAX + 1 ];
int typed_count = 0;
int last_code   = 0;        // last thing typed: its code,
bool last_text  = false;    // and whether it came as a character, not a key

// -----------------------------------------------------------------------------
//  Small helpers
// -----------------------------------------------------------------------------

int hex_digit( int value )
{
    if( value < 10 )
      return '0' + value;
    return 'A' + value - 10;
}

void colored( int color )
{
    set_multiply_color( color );
}

// one glyph, by BIOS code
void glyph_at( int x, int y, int code )
{
    int one[ 2 ];
    one[ 0 ] = code;
    one[ 1 ] = 0;
    print_at( x, y, one );
}

// "<number>" at (x, y)
void number_at( int x, int y, int value )
{
    int digits[ 12 ];
    itoa( value, digits, 10 );
    print_at( x, y, digits );
}

// A line of text made of one repeated code, with its two ends: the way a
// window's top and bottom edges are drawn (left, width times middle, right).
void rule_at( int x, int y, int left, int middle, int right, int width )
{
    int line[ 70 ];
    line[ 0 ] = left;
    for( int i = 1; i <= width; i++ )
      line[ i ] = middle;
    line[ width + 1 ] = right;
    line[ width + 2 ] = 0;
    print_at( x, y, line );
}

// -----------------------------------------------------------------------------
//  The page chrome: tabs on top, hints at the bottom
// -----------------------------------------------------------------------------

void draw_chrome()
{
    colored( page == 0 ? color_yellow : color_darkgray );
    print_at( 0, 0, "[F1] MAP" );
    colored( page == 1 ? color_yellow : color_darkgray );
    print_at( 100, 0, "[F2] COMPOSE" );
    colored( page == 2 ? color_yellow : color_darkgray );
    print_at( 230, 0, "[F3] SCENE" );

    if( keyboard.connected() )
    {
        colored( color_green );
        print_at( 400, 0, "keyboard: Gamepad 3" );
    }
    else
    {
        colored( color_red );
        print_at( 400, 0, "no keyboard: Gamepad 3" );
    }

    colored( color_white );
}

// -----------------------------------------------------------------------------
//  MAP: codes 32 to 255 in rows of 16, and 14 to 31 on the side
// -----------------------------------------------------------------------------

void draw_map()
{
    int line[ 40 ];

    colored( color_gray );
    print_at( 20, 40, "code" );
    for( int c = 0; c < 16; c++ )
      glyph_at( 70 + c * 20, 40, hex_digit( c ) );

    for( int r = 2; r < 16; r++ )
    {
        // ASCII white, the CP1252 punctuation cyan, the symbols yellow, the
        // letters green
        if( r < 8 )        colored( color_white );
        else if( r < 10 )  colored( color_cyan );
        else if( r < 12 )  colored( color_yellow );
        else               colored( color_green );

        for( int c = 0; c < 16; c++ )
        {
            int code = r * 16 + c;
            line[ c * 2 ] = code;
            // the codes the font has no character for are left blank
            if( code == 127 || code == 129 || code == 141 || code == 143 ||
                code == 144 || code == 157 )
              line[ c * 2 ] = ' ';
            line[ c * 2 + 1 ] = ' ';
        }
        line[ 31 ] = 0;
        print_at( 70, 40 + (r - 1) * 20, line );

        colored( color_gray );
        glyph_at( 20, 40 + (r - 1) * 20, hex_digit( r ) );
        glyph_at( 30, 40 + (r - 1) * 20, 'x' );
    }

    // codes 14 to 31 -- box drawing and shades
    colored( color_gray );
    print_at( 410, 40, "codes 14-31" );
    for( int group = 0; group < 3; group++ )
    {
        for( int i = 0; i < 6; i++ )
        {
            int code = 14 + group * 6 + i;
            colored( color_white );
            glyph_at( 420 + i * 36, 70 + group * 50, code );
            colored( color_darkgray );
            number_at( 414 + i * 36, 90 + group * 50, code );
        }
    }

    // the shades, in a ramp, and a frame made from the corners
    colored( color_white );
    rule_at( 420, 230, BOX_TL, BOX_H, BOX_TR, 8 );
    glyph_at( 420, 250, BOX_V );
    glyph_at( 420 + 90, 250, BOX_V );
    glyph_at( 430, 250, SHADE_1 );
    glyph_at( 440, 250, SHADE_1 );
    glyph_at( 450, 250, SHADE_2 );
    glyph_at( 460, 250, SHADE_2 );
    glyph_at( 470, 250, SHADE_3 );
    glyph_at( 480, 250, SHADE_3 );
    glyph_at( 490, 250, SOLID );
    glyph_at( 500, 250, SOLID );
    rule_at( 420, 270, BOX_BL, BOX_H, BOX_BR, 8 );

    colored( color_white );
    print_at( 400, 320, "ASCII" );
    colored( color_cyan );
    print_at( 460, 320, "80-9F" );
    colored( color_yellow );
    print_at( 520, 320, "A0-BF" );
    colored( color_green );
    print_at( 580, 320, "C0-FF" );
    colored( color_white );
}

// -----------------------------------------------------------------------------
//  COMPOSE: recipes, and a box to try them in
// -----------------------------------------------------------------------------

// add what was typed: a character the device sent, or a key with an ASCII one
void type_in( int code )
{
    if( typed_count >= TYPED_MAX )
    {
        // full: the oldest row scrolls away
        for( int i = 0; i < TYPED_MAX - TYPED_COLUMNS; i++ )
          typed[ i ] = typed[ i + TYPED_COLUMNS ];
        typed_count = TYPED_MAX - TYPED_COLUMNS;
    }
    typed[ typed_count ] = code;
    typed_count++;
}

void draw_compose()
{
    int row[ TYPED_COLUMNS + 2 ];

    colored( color_cyan );
    print_at( 0, 20, "On the adaptor (US layout), AltGr + a key:" );
    colored( color_white );
    // q:ä w:å e:é y:ü u:ú i:í o:ó p:ö
    print_at( 20, 40, "q:\344 w:\345 e:\351 y:\374 u:\372 i:\355 o:\363 p:\366" );
    // a:á s:ß d:ð l:ø z:æ n:ñ ,:ç c:©
    print_at( 20, 60, "a:\341 s:\337 d:\360 l:\370 z:\346 n:\361 ,:\347 c:\251" );
    colored( color_gray );
    // with Shift, capitals: Q:Ä E:É O:Ó P:Ö; and AltGr 5 is the euro: €
    print_at( 20, 80, "+ Shift: capitals (Q:\304 E:\311 O:\323 P:\326).  AltGr 5 = \200" );

    colored( color_cyan );
    print_at( 0, 100, "Dead keys, then a letter:" );
    colored( color_white );
    // AltGr+' e=é   AltGr+` a=à
    print_at( 20, 120, "AltGr+' e=\351   AltGr+` a=\340" );
    // AltGr+Shift+6 o=ô   AltGr+Shift+' u=ü   AltGr+Shift+` n=ñ
    print_at( 20, 140, "AltGr+Shift+6 o=\364   AltGr+Shift+' u=\374   AltGr+Shift+` n=\361" );

    colored( color_cyan );
    print_at( 0, 160, "Alt codes (left Alt, digits on the keypad):" );
    colored( color_white );
    // Alt 0233 = é   Alt 0128 = €   Alt 021 = ─   Alt 020 = █
    print_at( 20, 180, "Alt 0233 = \351   Alt 0128 = \200   Alt 021 = \025   Alt 020 = \024" );
    colored( color_gray );
    print_at( 0, 200, "Emulator: your own layout, dead keys and compose key." );

    // the box
    colored( color_darkgray );
    rule_at( 0, 220, BOX_TL, BOX_H, BOX_TR, TYPED_COLUMNS );
    for( int r = 0; r < TYPED_ROWS; r++ )
    {
        glyph_at( 0, 240 + r * 20, BOX_V );
        glyph_at( (TYPED_COLUMNS + 1) * 10, 240 + r * 20, BOX_V );
    }
    rule_at( 0, 300, BOX_BL, BOX_H, BOX_BR, TYPED_COLUMNS );

    colored( color_white );
    for( int r = 0; r < TYPED_ROWS; r++ )
    {
        for( int c = 0; c < TYPED_COLUMNS; c++ )
        {
            int at = r * TYPED_COLUMNS + c;
            if( at < typed_count )
              row[ c ] = typed[ at ];
            else if( at == typed_count && (get_frame_counter() / 30) % 2 == 0 )
              row[ c ] = '_';
            else
              row[ c ] = ' ';
        }
        row[ TYPED_COLUMNS ] = 0;
        print_at( 10, 240 + r * 20, row );
    }

    // what the last thing typed was
    colored( color_gray );
    print_at( 0, 340, "last:" );
    if( last_code > 0 )
    {
        colored( last_text ? color_green : color_yellow );
        print_at( 50, 340, last_text ? "CHARACTER" : "KEY      " );
        colored( color_white );
        number_at( 150, 340, last_code );
        if( last_code >= 14 && last_code != 127 )
          glyph_at( 200, 340, last_code );
    }
    colored( color_darkgray );
    print_at( 300, 340, "Backspace deletes, Enter clears" );
}

// -----------------------------------------------------------------------------
//  SCENE: a window of languages, symbols and shades
// -----------------------------------------------------------------------------

void draw_scene()
{
    int shade[ 60 ];
    int step  = get_frame_counter() / 4;
    int ramp[ 8 ] = { ' ', SHADE_1, SHADE_2, SHADE_3, SOLID, SHADE_3, SHADE_2, SHADE_1 };

    // the window's drop shadow (one cell right and down), then the window
    colored( color_darkgray );
    for( int r = 0; r < 15; r++ )
      glyph_at( 610, 40 + r * 20, SHADE_2 );
    rule_at( 30, 340, SHADE_2, SHADE_2, SHADE_2, 57 );

    colored( color_white );
    rule_at( 20, 20, BOX_TL, BOX_H, BOX_TR, 57 );
    for( int r = 0; r < 14; r++ )
    {
        glyph_at( 20, 40 + r * 20, BOX_V );
        glyph_at( 600, 40 + r * 20, BOX_V );
    }
    rule_at( 20, 320, BOX_BL, BOX_H, BOX_BR, 57 );

    // title in the top edge
    colored( color_yellow );
    print_at( 200, 40, " Glyphs of the BIOS font " );

    // the languages
    colored( color_white );
    // Où l'été a mûri, le cœur s'émeut: crème brûlée, façade
    print_at( 40, 60, "O\371 l'\351t\351 a m\373ri, le c\234ur s'\351meut: cr\350me br\373l\351e, fa\347ade" );
    colored( color_cyan );
    // Zwölf Boxkämpfer jagen Viktor quer über den Deich
    print_at( 40, 80, "Zw\366lf Boxk\344mpfer jagen Viktor quer \374ber den Deich" );
    colored( color_green );
    // ¿Qué hay, señor? ¡El pingüino añoró a la cigüeña!
    print_at( 40, 100, "\277Qu\351 hay, se\361or? \241El ping\374ino a\361or\363 a la cig\374e\361a!" );
    colored( color_yellow );
    // Høje Åse bærer ærlige grønne øl på Ærø
    print_at( 40, 120, "H\370je \305se b\346rer \346rlige gr\370nne \370l p\345 \306r\370" );
    colored( color_cyan );
    // Þórr og Óðinn drukku mjöð í Ásgarði
    print_at( 40, 140, "\336\363rr og \323\360inn drukku mj\366\360 \355 \301sgar\360i" );
    colored( color_green );
    // À noite, o vovô põe açúcar no chá de tâmaras
    print_at( 40, 160, "\300 noite, o vov\364 p\365e a\347\372car no ch\341 de t\342maras" );

    // symbols
    colored( color_white );
    // © ® ™ € £ ¥ ¢ ± × ÷ ° ¼ ½ ¾ « » ¿ ¡ § ¶ † ‡ • … ‰ – —
    print_at( 40, 200, "\251 \256 \231 \200 \243 \245 \242 \261 \327 \367 \260 \274 \275 \276 \253 \273 \277 \241 \247 \266 \206 \207 \225 \205 \211 \226 \227" );

    // a tree, in box drawing
    colored( color_gray );
    print_at( 40, 240, "cartridge" );
    // ├─ code      ├─ textures      └─ sounds
    print_at( 40, 260, "\033\025 code" );
    print_at( 40, 280, "\033\025 textures" );
    print_at( 40, 300, "\032\025 sounds" );

    // bars made of the shades, sliding along in opposite directions
    for( int i = 0; i < 40; i++ )
      shade[ i ] = ramp[ (i + step) % 8 ];
    shade[ 40 ] = 0;
    colored( color_cyan );
    print_at( 190, 260, shade );
    for( int i = 0; i < 40; i++ )
      shade[ i ] = ramp[ (i + 8 - step % 8) % 8 ];   // never negative
    colored( color_yellow );
    print_at( 190, 280, shade );
    colored( color_white );
}

// -----------------------------------------------------------------------------
//  Main
// -----------------------------------------------------------------------------

void turn_to( int to )
{
    page = (to + PAGES) % PAGES;
    idle = 0;
}

// a key that changes page (the others are for the COMPOSE page's box)
void handle_key( int key )
{
    idle = 0;
    if( key == v32::KeyF1 )         turn_to( 0 );
    else if( key == v32::KeyF2 )    turn_to( 1 );
    else if( key == v32::KeyF3 )    turn_to( 2 );
    else if( key == v32::KeyTab )   turn_to( page + 1 );
    else if( key == v32::KeyRight ) turn_to( page + 1 );
    else if( key == v32::KeyLeft )  turn_to( page - 1 );
    else if( key == v32::KeyBackspace && page == 1 )
    {
        if( typed_count > 0 )
          typed_count--;
    }
    else if( key == v32::KeyEnter && page == 1 )
      typed_count = 0;
}

void main()
{
    v32::KeyEvent event;

    while( true )
    {
        // once on every frame
        keyboard.probe();

        while( keyboard.next_event( &event ) )
        {
            if( !event.pressed )
              continue;

            if( event.text )
            {
                // a typed character: key is its BIOS font code, ready to draw
                last_code = event.key;
                last_text = true;
                idle      = 0;
                type_in( event.key );
            }
            else
            {
                last_code = event.key;
                last_text = false;
                handle_key( event.key );
                // a key with an ASCII character: the symbol is what it typed
                if( event.symbol >= 32 && event.symbol < 127 )
                {
                    idle = 0;
                    type_in( event.symbol );
                }
            }
        }

        // an ordinary gamepad on Gamepad 1 pages through. The console starts
        // with the first CONNECTED gamepad selected, which can be the
        // keyboard's own: select Gamepad 1 before reading its buttons
        select_gamepad( 0 );
        if( gamepad_left() == 1 || gamepad_button_l() == 1 )
          turn_to( page - 1 );
        if( gamepad_right() == 1 || gamepad_button_r() == 1 || gamepad_button_a() == 1 )
          turn_to( page + 1 );

        // with nobody there, turn the page now and then -- but not away from
        // the typing box while a keyboard is there to type in it
        idle++;
        if( idle >= AUTO_FRAMES && !(page == 1 && keyboard.connected()) )
          turn_to( page + 1 );

        clear_screen( color_black );
        draw_chrome();
        if( page == 0 )       draw_map();
        else if( page == 1 )  draw_compose();
        else                  draw_scene();

        end_frame();
    }
}
