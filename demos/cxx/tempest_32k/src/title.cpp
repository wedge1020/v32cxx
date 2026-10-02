// *****************************************************************************
//  TEMPEST 32K -- src/title.cpp
//  the title screen
// *****************************************************************************
#include "tempest.hpp"

// ---------------------------------------------------------------------------
//  Title screen — block-letter "TEMPEST 32K" built from BIOS regions
//  17-20 (graded 10x20 blocks: 20 solid down to 17 lightest), undulating
//  horizontally with the wave phase picking the shade, plus a slow hue
//  cycle. Menu underneath (PLAY / HIGH SCORES / LEVEL SELECT /
//  DIFFICULTY); all options wave, the selected one rides a bigger wave
//  and color-cycles.
// ---------------------------------------------------------------------------
char* title_bitmap( int c )
{
    // 5x7 pixel font, row-major, one 35-char string per glyph
    if( c == 'T' ) return "11111001000010000100001000010000100";
    if( c == 'E' ) return "11111100001000011110100001000011111";
    if( c == 'M' ) return "10001110111010110101100011000110001";
    if( c == 'P' ) return "11110100011000111110100001000010000";
    if( c == 'S' ) return "01111100001000001110000010000111110";
    if( c == '3' ) return "11110000010000101110000010000111110";
    if( c == '2' ) return "11110000010000100110010001000011111";
    if( c == 'K' ) return "10001100101010011000101001001010001";
    return "00000000000000000000000000000000000";   // space
}

void render_title( G* g )
{
    char* word = "TEMPEST 32K";
    float t = g->frame * 0.06;
    int i = 0; int r; int c;

    set_blending_mode( BLEND_SOLID );
    set_drawing_scale( 0.8, 0.4 );     // one scale for all 8x8 block cells
    int creg = -1;                     // region cache (raw draws, not glyphs)

    while( word[ i ] != 0 )
    {
        char* bm = title_bitmap( word[ i ] );
        // slow hue cycle, offset per letter so the rainbow drifts along
        set_multiply_color( hue( g->frame + i * 22 ) );
        for( r = 0; r < 7; r++ )
        {
            for( c = 0; c < 5; c++ )
            {
                if( bm[ r * 5 + c ] != '1' ) continue;
                int colg = i * 5 + c;
                float wv = SIN32( g, colg * 0.5 + t );
                int reg = 17;
                if( wv > 0.55 )       reg = 20;   // crest: solid
                else if( wv > 0.0 )   reg = 19;
                else if( wv > -0.55 ) reg = 18;
                if( reg != creg ) { select_region( reg ); creg = reg; }
                float bx = 56 + i * 48 + c * 8 + wv * 5.0;
                float by = 48 + r * 8 + SIN32( g, colg * 0.28 + t * 0.7 ) * 3.5;
                draw_region_zoomed_at( (int)( bx ), (int)( by ) );
                g->cpu_cycles += 24 + 64;   // CPU meter: 8x8 block fill
            }
        }
        i++;
    }
    // raw region draws invalidate the draw_glyph cache
    g->last_region = -1;
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;

    // menu — stacked, centered; every option undulates, the selection
    // gets the bigger wave + hue cycle. Rows 1 and 4 are selectors
    // (PLAYERS / DIFFICULTY): LEFT/RIGHT changes the value while the
    // row is selected; A on PLAY starts the game with that setup.
    char* rows[ 5 ];
    rows[ 0 ] = "PLAY";
    if( g->players == 2 ) rows[ 1 ] = "PLAYERS: 2";
    else                   rows[ 1 ] = "PLAYERS: 1";
    rows[ 2 ] = "HIGH SCORES";
    rows[ 3 ] = "LEVEL SELECT";
    if( g->difficulty == 0 )      rows[ 4 ] = "DIFFICULTY: EASY";
    else if( g->difficulty == 1 ) rows[ 4 ] = "DIFFICULTY: MEDIUM";
    else                          rows[ 4 ] = "DIFFICULTY: HARD";
    int d; int ci;
    for( d = 0; d < 5; d++ )
    {
        float sel = 0;
        if( d == g->menu_row ) sel = 1;
        float msz = 14 + sel * 4;
        // letter spacing widened: unselected 0.78 (the old 0.62 read
        // as squeezed), selected gets an extra-wide 0.90 for pop
        float adv = msz * 0.78;
        if( sel > 0 ) adv = msz * 0.9;
        // centring offset: measured once per row (it used to be
        // re-measured for every letter -- quadratic in the row length)
        float left = 320 - ( string_len( rows[ d ] ) - 1 ) * 0.5 * adv;
        ci = 0;
        while( rows[ d ][ ci ] != 0 )
        {
            float wob = SIN32( g, ci * 0.6 + g->frame * 0.04 + d ) * ( 2 + sel * 4 );
            if( d == g->menu_row )
                set_multiply_color( hue( g->frame + ci * 14 ) );
            else
                set_multiply_color( RGB( 90, 100, 125 ) );
            draw_glyph( g, rows[ d ][ ci ], left + ci * adv,
                        140 + d * 30 + wob, msz, msz * 1.2 );
            ci++;
        }
    }

    // gameplay controls legend — back to 12px: the vertical oversample
    // makes small text render fully, so the earlier size bump isn't
    // needed. X re-centered for the 0.78 advance.
    draw_text( g, "A: FIRE  B: SUPERZAP  Y: JUMP",
               184, 290, 12, RGB( 120, 130, 160 ) );

    // controls hint
    draw_text( g, "UP/DOWN: SELECT  LEFT/RIGHT: CHANGE  A: GO",
               109, 320, 12, RGB( 120, 130, 160 ) );
}
