// *****************************************************************************
//  TEMPEST 32K -- src/hud.cpp
//  HUD, messages, pause menu, initials entry, high scores, level select
// *****************************************************************************
#include "tempest.hpp"

void render_hud( G* g )
{
    // big rainbow score at top — pure Tempest 2000
    char buf[ 16 ];
    char tmp[ 16 ];
    int n = g->score;
    int i = 0;
    int len = 0;
    if( n == 0 ) { tmp[ 0 ] = '0'; len = 1; }
    else
    {
        while( n > 0 && i < 15 )
        {
            tmp[ i ] = '0' + ( n % 10 );
            n /= 10;
            i++;
        }
        len = i;
        for( i = 0; i < len; i++ ) buf[ i ] = tmp[ len - 1 - i ];
        for( i = 0; i < len; i++ ) tmp[ i ] = buf[ i ];
        tmp[ len ] = 0;
    }
    // score top-right, right-aligned: top-center collided with the web
    // rim and incoming enemies
    draw_party_text( g, tmp, 628 - len * 16 * 0.31, 18, 16, 0 );

    // co-op: P2's tally rides directly beneath P1's, hue-shifted to
    // the P2 claw's cyan so the two scores read apart at a glance
    if( g->twoplayer )
    {
        char tmp2[ 16 ];
        char buf2[ 16 ];
        int n2 = g->p2_score;
        int i2 = 0;
        int len2 = 0;
        if( n2 == 0 ) { tmp2[ 0 ] = '0'; len2 = 1; }
        else
        {
            while( n2 > 0 && i2 < 15 )
            {
                tmp2[ i2 ] = '0' + ( n2 % 10 );
                n2 /= 10;
                i2++;
            }
            len2 = i2;
            for( i2 = 0; i2 < len2; i2++ ) buf2[ i2 ] = tmp2[ len2 - 1 - i2 ];
            for( i2 = 0; i2 < len2; i2++ ) tmp2[ i2 ] = buf2[ i2 ];
            tmp2[ len2 ] = 0;
        }
        draw_party_text( g, tmp2, 628 - len2 * 16 * 0.31, 46, 16, 160 );
    }

    // lives as spare claws: the brace ship (region 123), aimed at the
    // web's centre so the icons echo the player sprite ('X' now
    // belongs to the walker)
    // ANGLE CACHE: every icon slot is at a fixed screen position, so
    // each slot's centre-aim angle is a CONSTANT — computed once
    // (8 slots total; the lives cap is 4 per player), replacing 8
    // forty-iteration dir_angle table scans per frame.
    if( g->hud_ang_ok == 0 )
    {
        int lc;
        for( lc = 0; lc < 8; lc++ )
        {
            float ix1 = 560 + lc * 22;
            float ix2 = 100 + lc * 22;
            g->HUD1_ANG[ lc ] = dir_angle( g, CX - ix1, CY - 346 ) * 0.024543692;
            g->HUD2_ANG[ lc ] = dir_angle( g, CX - ix2, CY - 346 ) * 0.024543692;
        }
        g->hud_ang_ok = 1;
    }
    int l;
    set_blending_mode( BLEND_SOLID );
    set_multiply_color( RGB( 255, 220, 60 ) );
    for( l = 0; l < g->lives; l++ )
    {
        draw_rot_glyph( g, 123, 560 + l * 22, 346, 12, 16, g->HUD1_ANG[ l ] );
    }

    // P2's spare claws: cyan braces on the left half of the HUD row,
    // clear of the LVL readout and the superzap charges
    if( g->twoplayer )
    {
        set_multiply_color( RGB( 60, 220, 255 ) );
        for( l = 0; l < g->p2_lives; l++ )
        {
            draw_rot_glyph( g, 123, 100 + l * 22, 346, 12, 16, g->HUD2_ANG[ l ] );
        }
    }

    // level
    char lvl[ 7 ];
    lvl[ 0 ] = 'L'; lvl[ 1 ] = 'V'; lvl[ 2 ] = 'L'; lvl[ 3 ] = ' ';
    lvl[ 4 ] = '0' + ( g->level / 10 ) % 10;
    lvl[ 5 ] = '0' + g->level % 10;
    lvl[ 6 ] = 0;
    draw_text( g, lvl, 20, 346, 12, RGB( 120, 200, 255 ) );

    // superzapper charges — bottom row spread out so the pod timers
    // (AI / RAPID / LASER) never crowd them or each other. Row sits
    // at y=346: the screen is 640x360, and the 336-360 band is free
    // space (stars streak through it).
    set_multiply_color( RGB( 255, 255, 255 ) );
    for( l = 0; l < g->superzaps; l++ )
        draw_glyph( g, 'Z', 240 + l * 20, 346, 12, 14.4 );

    // AI buddy countdown (seconds remaining) while it is online
    if( g->buddy_timer > 0 )
    {
        char bud[ 6 ];
        int secs = g->buddy_timer / 60;
        bud[ 0 ] = 'A'; bud[ 1 ] = 'I'; bud[ 2 ] = ' ';
        bud[ 3 ] = '0' + ( secs / 10 ) % 10;
        bud[ 4 ] = '0' + secs % 10;
        bud[ 5 ] = 0;
        draw_text( g, bud, 316, 346, 12, RGB( 120, 255, 160 ) );
    }

    // rapid blaster countdown while active (blinks in the final
    // seconds). Easy's unlimited pod shows no number.
    if( g->rapid_timer > 0 )
    {
        if( g->rapid_timer > 180 || ( g->frame % 8 ) < 4 )
        {
            char rap[ 10 ];
            rap[ 0 ] = 'R'; rap[ 1 ] = 'A'; rap[ 2 ] = 'P';
            rap[ 3 ] = 'I'; rap[ 4 ] = 'D';
            int rp = 5;
            int rsecs = g->rapid_timer / 60;
            if( rsecs < 100 )
            {
                rap[ 5 ] = ' ';
                rap[ 6 ] = '0' + ( rsecs / 10 ) % 10;
                rap[ 7 ] = '0' + rsecs % 10;
                rp = 8;
            }
            rap[ rp ] = 0;
            draw_text( g, rap, 386, 346, 12, RGB( 255, 160, 60 ) );
        }
    }

    // super laser countdown while active (blinks in the final seconds)
    if( g->laser_timer > 0 )
    {
        if( g->laser_timer > 180 || ( g->frame % 8 ) < 4 )
        {
            char las[ 9 ];
            int secs = g->laser_timer / 60;
            las[ 0 ] = 'L'; las[ 1 ] = 'A'; las[ 2 ] = 'S'; las[ 3 ] = 'E';
            las[ 4 ] = 'R'; las[ 5 ] = ' ';
            las[ 6 ] = '0' + ( secs / 10 ) % 10;
            las[ 7 ] = '0' + secs % 10;
            las[ 8 ] = 0;
            draw_text( g, las, 470, 346, 12, RGB( 80, 220, 255 ) );
        }
    }
}

void render_message( G* g )
{
    if( g->message_timer <= 0 ) return;
    draw_party_text( g, g->message, 320, 70, 22, 90 );
    g->message_timer--;
}

// pause veil: translucent dark overlay (region 20 + alpha — the proven
// path) over the frozen scene, volume meters, track list. Music keeps
// playing; nothing else updates (gated in main).

// 10-segment volume meter at (x,y); 0..2 maps to 0..10 blocks
void draw_meter( G* g, int x, int y, float vol )
{
    int segs = (int)( vol * 5.0 + 0.5 );
    if( segs > 10 ) segs = 10;
    int s;
    for( s = 0; s < 10; s++ )
    {
        select_region( 20 );
        set_drawing_scale( 2.0, 0.5 );          // 20 x 10 px blocks
        if( s < segs ) set_multiply_color( RGB( 90, 220, 120 ) );
        else          set_multiply_color( RGB( 30, 45, 40 ) );
        draw_region_zoomed_at( x + s * 22, y );
        g->cpu_cycles += 24 + 200;   // CPU meter: 20x10 block fill
    }
    g->last_region = -1;
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;
}

void render_pause( G* g )
{
    set_blending_mode( v32::BlendAlpha );
    select_region( 20 );
    set_drawing_scale( 64.0, 18.0 );          // 640 x 360 fullscreen veil
    set_multiply_color( RGB( 8, 8, 26 ) );
    draw_region_zoomed_at( 0, 0 );
    // CPU meter: a full-screen fill is nearly the whole frame budget
    // in this model — the paused readout will read ~90%+
    g->cpu_cycles += 24 + 230400;
    g->last_region = -1;
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;
    set_blending_mode( BLEND_SOLID );

    draw_party_text( g, "PAUSED", 320, 44, 22, 90 );

    // separate meters: MUSIC (channel 0) and SFX (channels 2..13).
    // Rows are spaced for the 1.2x-tall oversampled glyphs — the old
    // 12px/18px-pitch layout read as clipped, crowded lines.
    draw_text( g, "MUSIC", 240, 84, 14, RGB( 150, 160, 190 ) );
    draw_meter( g, 220, 106, g->music_volume );
    draw_text( g, "SFX", 240, 140, 14, RGB( 150, 160, 190 ) );
    draw_meter( g, 220, 162, g->sfx_volume );

    // track list
    draw_text( g, "TRACK", 240, 196, 14, RGB( 150, 160, 190 ) );
    char* tracks[ 4 ];
    tracks[ 0 ] = "1 WEB CRAWLER";
    tracks[ 1 ] = "2 SPIKE SURFER";
    tracks[ 2 ] = "3 HYPERSPACE";
    tracks[ 3 ] = "4 FLIPPER STORM";
    int t;
    for( t = 0; t < 4; t++ )
    {
        int colr;
        if( t == g->music_index ) colr = hue( g->frame + t * 20 );
        else                      colr = RGB( 90, 100, 125 );
        draw_text( g, tracks[ t ], 250, 220 + t * 22, 14, colr );
    }

    draw_text( g, "START: RESUME  LEFT/RIGHT: MUSIC  L/R: SFX  UP/DOWN: TRACK",
               49, 318, 12, RGB( 120, 130, 160 ) );
}

// ---------------------------------------------------------------------------
//  High score screens — initials entry (state 6) and the table (state 7)
// ---------------------------------------------------------------------------

// state 6: "NEW HIGH SCORE" — big three-slot initials entry
void render_entry( G* g )
{
    draw_party_text( g, "NEW HIGH SCORE!", 320, 70, 18, 300 );
    char sbuf[ 12 ];
    score_str( g->score, &sbuf[ 0 ] );
    draw_party_text( g, &sbuf[ 0 ], 320, 120, 22, 40 );

    // three big letter slots; the cursor slot hue-cycles
    int i;
    for( i = 0; i < 3; i++ )
    {
        char ch[ 2 ];
        ch[ 0 ] = g->entry_letters[ i ];
        ch[ 1 ] = 0;
        int colr = RGB( 90, 100, 125 );
        if( i == g->entry_pos ) colr = hue( g->frame );
        draw_text( g, &ch[ 0 ], 320 + ( i - 1 ) * 56 - 10, 180, 30, colr );
    }
    draw_text( g, "ENTER YOUR INITIALS", 231, 148, 12, RGB( 120, 130, 160 ) );
    draw_text( g, "UP/DOWN: LETTER  LEFT/RIGHT: MOVE  A: OK",
               133, 280, 12, RGB( 120, 130, 160 ) );
}

// state 7: the stored table, top 5
void render_scores( G* g )
{
    draw_party_text( g, "HIGH SCORES", 320, 56, 20, 80 );
    int i;
    for( i = 0; i < 5; i++ )
    {
        int y = 130 + i * 32;
        int colr = RGB( 120, 130, 160 );
        if( i == 0 ) colr = hue( g->frame );
        char rk[ 3 ];
        rk[ 0 ] = '1' + i;
        rk[ 1 ] = '.';
        rk[ 2 ] = 0;
        draw_text( g, &rk[ 0 ], 246, y, 14, colr );
        // initials: drawn one letter at a time with a WIDER advance —
        // even the widened row advance (size * 0.78) sits 14px glyphs
        // in 10.9px slots; the 15px pitch here keeps them airy
        int j;
        for( j = 0; j < 3; j++ )
        {
            char ch[ 2 ];
            ch[ 0 ] = g->HIINIT[ i ][ j ];
            ch[ 1 ] = 0;
            draw_text( g, &ch[ 0 ], 288 + j * 15, y, 14, colr );
        }
        char sbuf[ 12 ];
        score_str( g->HISCORE[ i ], &sbuf[ 0 ] );
        draw_text( g, &sbuf[ 0 ], 348, y, 14, colr );
    }
    draw_text( g, "A: BACK", 287, 326, 12, RGB( 120, 130, 160 ) );
}

// state 8: level select — a slowly ROTATING wireframe of the chosen
// level's web silhouette (real SHAPE data + real CONN gaps), LEFT/RIGHT
// cycles the level (1..32). Sixteen webs; each is visited twice across
// the 32 levels, the second pass playing harder.
void render_levelselect( G* g )
{
    draw_party_text( g, "LEVEL SELECT", 320, 46, 18, 200 );
    char lbuf[ 9 ];
    lbuf[ 0 ] = 'L'; lbuf[ 1 ] = 'E'; lbuf[ 2 ] = 'V'; lbuf[ 3 ] = 'E';
    lbuf[ 4 ] = 'L'; lbuf[ 5 ] = ' ';
    lbuf[ 6 ] = '0' + ( g->select_level / 10 ) % 10;
    lbuf[ 7 ] = '0' + g->select_level % 10;
    lbuf[ 8 ] = 0;
    draw_party_text( g, &lbuf[ 0 ], 320, 100, 16, 0 );

    // rotating web preview: the real SHAPE outline spinning about the
    // screen center (same draw_segment bar path the in-game web uses)
    float spin = g->frame * 0.012;
    float cx = 320;
    float cy = 235;
    float rx = 150;
    float ry = 105;
    int i;
    set_blending_mode( v32::BlendAlpha );
    for( i = 0; i < LANES; i++ )
    {
        if( !g->CONN[ i ] ) continue;   // open webs: mirror the rim gaps
        int j = i + 1;
        if( j >= LANES ) j -= LANES;
        float a0 = spin + i * 0.392699081;   // i * PI/8
        float a1 = spin + j * 0.392699081;
        set_multiply_color( hue( g->frame + i * 16 ) );
        draw_segment( g,
                      cx + cos32( g, a0 ) * rx * g->SHAPE[ i ],
                      cy + sin32( g, a0 ) * ry * g->SHAPE_Y[ i ],
                      cx + cos32( g, a1 ) * rx * g->SHAPE[ j ],
                      cy + sin32( g, a1 ) * ry * g->SHAPE_Y[ j ],
                      5 );
    }
    g->last_region = -1;
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;

    // depth ring of dots at the tube's far end
    set_blending_mode( BLEND_SOLID );
    set_multiply_color( RGB( 90, 100, 130 ) );
    for( i = 0; i < LANES; i++ )
    {
        float a = spin + i * 0.392699081 + 0.196;   // half-step between bars
        draw_glyph( g, '.',
                    cx + cos32( g, a ) * rx * g->SHAPE[ i ] * 0.45,
                    cy + sin32( g, a ) * ry * g->SHAPE_Y[ i ] * 0.45, 3, 3 );
    }
    g->last_region = -1;
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;

    draw_text( g, "LEFT/RIGHT: LEVEL  A: START  B: BACK",
               147, 326, 12, RGB( 120, 130, 160 ) );
}
