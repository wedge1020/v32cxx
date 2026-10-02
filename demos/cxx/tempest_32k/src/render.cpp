// *****************************************************************************
//  TEMPEST 32K -- src/render.cpp
//  gameplay rendering: web, spikes, claws, enemies, bullets, effects
// *****************************************************************************
#include "tempest.hpp"

// ---------------------------------------------------------------------------
//  Rendering
// ---------------------------------------------------------------------------
void render_web( G* g )
{
    int i; int r;
    // WARP ECONOMY: level transitions are the frame budget's worst
    // moment — the whole web streams outward, the starfield streaks
    // and the parity-split rings RACE (scroll grows with warp^2) and
    // strobe visibly. During the transition the garnish sits out:
    // no ring dots, no seam-hiding vertex caps. The spokes, cap and
    // rim bars plus the streaking stars fully sell the motion.
    int warping = g->warp > 0 ? 1 : 0;

    // WEB BAR ANGLE CACHE: the rim and far-cap bar DIRECTIONS are
    // per-level constants — camera sway shifts both endpoints of a
    // bar by the same amount (it is z-proportional and both endpoints
    // share z), the jump pull-back and the warp fly scale both
    // endpoints uniformly, so a bar's direction changes ONLY when the
    // web shape does. make_shape invalidates; this block refills the
    // cache once per level (32 segment_angle calls total, replacing
    // 32 forty-iteration dir_angle scans EVERY frame — the single
    // biggest CPU win in the web renderer). Spokes and spikes span
    // z (camera sway bends them), so they keep computing angles.
    // WEB POINT CACHE: every bar, spoke and vertex cap below starts and
    // ends on a rim (z=0) or far-cap (z=1) vertex, so project each of
    // those 32 points ONCE per frame. Lane 16 is lane 0 (the web wraps),
    // hence the `& ( LANES - 1 )` on every "next lane" index below.
    for( i = 0; i < LANES; i++ )
    {
        project( g, i, 0 );
        g->RIM_X[ i ] = g->px; g->RIM_Y[ i ] = g->py;
        project( g, i, 1 );
        g->CAP_X[ i ] = g->px; g->CAP_Y[ i ] = g->py;
    }

    if( g->web_ang_ok == 0 )
    {
        int j;
        for( j = 0; j < LANES; j++ )
        {
            int k = ( j + 1 ) & ( LANES - 1 );
            g->RIM_ANG[ j ] = segment_angle( g, g->RIM_X[ k ] - g->RIM_X[ j ],
                                                g->RIM_Y[ k ] - g->RIM_Y[ j ] );
            g->CAP_ANG[ j ] = segment_angle( g, g->CAP_X[ k ] - g->CAP_X[ j ],
                                                g->CAP_Y[ k ] - g->CAP_Y[ j ] );
        }
        g->web_ang_ok = 1;
    }

    // WEB BARS DRAW UNDER ALPHA, not mode 0: the one draw_segment
    // path proven to render on the real emulator is the spikes'
    // (set_glow -> BlendAlpha). Mode-0 draws of region 20 with a
    // custom hotspot have never appeared. Opaque colors + alpha
    // blending = same visual result as solid for our purposes.
    set_blending_mode( v32::BlendAlpha );

    // lane edges (SPOKES): production build -- dim teal-blue, thickness 2.
    set_multiply_color( RGB( 25, 70, 110 ) );
    for( i = 0; i < LANES; i++ )
        draw_segment( g, g->RIM_X[ i ], g->RIM_Y[ i ], g->CAP_X[ i ], g->CAP_Y[ i ], 2.0 );

    // far end cap: full outline, every lane joined (16 bars) -- 2-lane
    // chords cut corners and left gaps that read as detached segments
    set_multiply_color( RGB( 40, 90, 130 ) );
    for( i = 0; i < LANES; i++ )
    {
        if( !g->CONN[ i ] ) continue;   // open web: cap mirrors the rim gap
        int k = ( i + 1 ) & ( LANES - 1 );
        draw_segment_c( g, g->CAP_X[ i ], g->CAP_Y[ i ], g->CAP_X[ k ], g->CAP_Y[ k ],
                        2.0, g->CAP_ANG[ i ] );
    }

    // far-cap vertex caps: the cap polygon is tiny, so a 1-2px angular
    // miss at a spoke joint reads as a clear disconnect (most visible
    // on the right side). Small blocks at each cap vertex cover the
    // joints, matching the rim's point markers. 16 cheap zoomed draws.
    if( !warping )
    {
        select_region( 20 );
        set_drawing_scale( 0.5, 0.35 );     // ~5 x 7 px block
        for( i = 0; i < LANES; i++ )
        {
            draw_region_zoomed_at( (int)( g->CAP_X[ i ] - 3 ), (int)( g->CAP_Y[ i ] - 3 ) );
            g->cpu_cycles += 24 + 35;   // CPU meter: 5x7 block fill
        }
        g->last_region = -1;
        g->last_scale_x = -9999.0;
    }

    // depth rings: cheap zoomed DOTS (the rotozoomed command is the
    // emulator's slow path -- rings move every frame, so they get the
    // budget treatment). EVERY-OTHER-DOT PARITY: each frame draws
    // half the dots, the complementary half next frame -- the ring
    // reads as a steady shimmering circle at HALF the draw cost
    // (56/frame instead of 112, the second-biggest consumer after
    // particles). Heavy salvos thin it further via ringstep.
    // Skipped during warp: racing dots strobe under the parity split.
    if( !warping )
    {
        int ringstep = 1;
        if( g->partcount > 120 ) ringstep = 2;
        float scroll = 0.002 + g->warp * g->warp * 0.12;
        for( r = 0; r < RINGS; r++ )
        {
            float z = ( ( g->frame * scroll ) + r * ( 1.0 / RINGS ) );
            while( z > 1 ) z -= 1;
            set_multiply_color( make_color( 20, 60 + r * 6, 90 ) );
            float s = 3.0 + z * 3.0;
            for( i = 0; i < LANES; i += ringstep )
            {
                if( ( i + g->frame ) % 2 == 1 ) continue;   // parity split
                project( g, i + 0.5, z );
                draw_glyph( g, '.', g->px, g->py, s, s );
            }
        }
    }

    // the rim: bright neon outline, every lane joined (16 bars), pulsing
    int pulse = 200 + (int)( SIN32( g, g->frame * 0.1 ) * 55 );
    set_multiply_color( make_color( 60, 180, pulse ) );
    for( i = 0; i < LANES; i++ )
    {
        if( !g->CONN[ i ] ) continue;   // open web: no bar over the gap
        int k = ( i + 1 ) & ( LANES - 1 );
        draw_segment_c( g, g->RIM_X[ i ], g->RIM_Y[ i ], g->RIM_X[ k ], g->RIM_Y[ k ],
                        3.5, g->RIM_ANG[ i ] );
    }

    // vertex caps: a small block at each rim vertex, exactly like the
    // point markers on Tempest's web. Hides the seam gap at the top
    // wrap (lane 15 -> 16) and caps any chord ends that stick out
    // past a joint. 16 cheap zoomed draws. Skipped mid-warp.
    if( !warping )
    {
        select_region( 20 );
        set_drawing_scale( 0.55, 0.3 );     // ~5.5 x 6 px block
        for( i = 0; i < LANES; i++ )
        {
            draw_region_zoomed_at( (int)( g->RIM_X[ i ] - 3 ), (int)( g->RIM_Y[ i ] - 3 ) );
            g->cpu_cycles += 24 + 33;   // CPU meter: 5.5x6 block fill
        }
        g->last_region = -1;
        g->last_scale_x = -9999.0;
    }
}

// spikes: solid red bars from the far end down toward the rim
void render_spikes( G* g )
{
    int i;
    set_glow( 2 );
    set_multiply_color( RGB( 255, 60, 30 ) );
    for( i = 0; i < LANES; i++ )
    {
        if( g->SPIKE[ i ] <= 0 ) continue;
        project( g, i, 1.0 - g->SPIKE[ i ] );  float x0 = g->px; float y0 = g->py;
        // the base sits on the far cap: render_web cached that point
        draw_segment( g, x0, y0, g->CAP_X[ i ], g->CAP_Y[ i ], 3.0 );
        draw_glyph( g, '+', x0, y0, 8, 8 );     // hot tip
    }
    set_blending_mode( BLEND_SOLID );
}

// one claw, any player: who = 0 (P1, gold) or 1 (P2, cyan). Both
// share the jump-arc pop and the rim -> far-cap aim; in co-op a
// respawning claw blinks and a player who is OUT is not drawn.
void render_claw( G* g, int who )
{
    float lane = g->player_lane;
    int jt = g->jump_timer;
    int rsp = g->p1_respawn;
    int bcol = RGB( 255, 220, 60 );
    int ccol = RGB( 255, 120, 40 );
    if( who == 1 )
    {
        lane = g->p2_lane;
        jt = g->p2_jump_timer;
        rsp = g->p2_respawn;
        bcol = RGB( 60, 220, 255 );
        ccol = RGB( 120, 60, 255 );
    }
    if( rsp > 0 && ( g->frame % 8 ) < 4 ) return;   // respawn blink

    // transition rework: the claw NO LONGER dives into the tube on
    // warp-out — it parks on the rim while the web streams past it.
    // project() has the fly factor baked in, so divide it back out.
    project( g, lane, 0 );
    float x = g->px; float y = g->py; float s = g->pscale;
    float fly = fly_factor( g );
    x = CX + ( x - CX ) / fly;
    y = CY + ( y - CY ) / fly;

    // jump arc: airborne claw pops toward the camera — bigger and
    // pushed radially outward from the tube axis (T2K-style leap)
    if( jt > 0 )
    {
        float jarc = sin32( g, 3.14159 * ( 34 - jt ) / 34.0 );
        x += ( x - CX ) * jarc * 0.18;
        y += ( y - CY ) * jarc * 0.18;
        s *= ( 1.0 + jarc * 1.2 );
    }

    // THE CLAW IS BIOS REGION 123 — the left-curly-brace glyph: it
    // aims down the lane spoke — the exact direction the shots
    // travel — so the ship visibly points where it fires.
    project( g, lane, 1 );
    float capx = CX + ( g->px - CX ) / fly;
    float capy = CY + ( g->py - CY ) / fly;
    float aim = dir_angle( g, capx - x, capy - y ) * 0.024543692;

    set_blending_mode( BLEND_SOLID );
    set_multiply_color( bcol );
    draw_rot_glyph( g, 123, x, y, 24 * s, 30 * s, aim );
    // hot core, glowing, riding the same rotation — CULLED on busy
    // frames (explosion salvos) exactly like the enemy glow cores
    if( g->partcount <= 160 )
    {
        set_glow( 3 );
        set_multiply_color( ccol );
        draw_rot_glyph( g, 123, x, y, 12 * s, 18 * s, aim );
    }
    set_blending_mode( BLEND_SOLID );
}

void render_player( G* g )
{
    if( g->state == 1 ) return;   // dying: particles only
    if( !g->p1_out ) render_claw( g, 0 );
    if( g->twoplayer && !g->p2_out ) render_claw( g, 1 );
}

void render_enemies( G* g )
{
    // BUSY-FRAME CULL: each enemy's glow core is a second draw per
    // enemy. During heavy explosion salvos skip them — the body glyph
    // still reads, and each iteration resets blending to solid at the
    // top so a skipped core never leaks state into the next enemy.
    // The warp transition forces the skip too: level changes are the
    // frame budget's tightest moment.
    int busyp = g->partcount;
    if( g->warp > 0 ) busyp = 999;
    int i;
    for( i = 0; i < MAX_ENEMIES; i++ )
    {
        if( !g->ENEMIES[ i ].alive ) continue;
        project( g, g->ENEMIES[ i ].lane, g->ENEMIES[ i ].z );
        float x = g->px; float y = g->py; float s = g->pscale;
        // VISIBILITY: pscale bottoms out at 0.10 at the far cap, which
        // made climbers 2-3 px specks — invisible until dangerously
        // close. Lift the far end hard (x4) and the near end slightly
        // (x1.15) so depth is still sold but every enemy stays readable
        float sv = ( s + ( 1.0 - s ) * 0.40 ) * 1.15;

        float wob = 0;
        if( g->ENEMIES[ i ].z <= 0 )
            wob = sin32( g, g->ENEMIES[ i ].wig + g->frame * 0.2 ) * 4;

        set_blending_mode( BLEND_SOLID );
        if( g->ENEMIES[ i ].type == 0 )
        {
            set_multiply_color( hue( 160 + ( ( i * 40 + g->frame ) >> 2 ) ) );
            draw_glyph( g, 'W', x, y + wob, 26 * sv, 20 * sv );
            if( busyp <= 160 )
            {
                set_glow( 4 );
                draw_glyph( g, '*', x, y + wob, 14 * sv, 14 * sv );
            }
        }
        else if( g->ENEMIES[ i ].type == 1 )
        {
            set_multiply_color( RGB( 255, 80, 160 ) );
            draw_glyph( g, 'H', x, y, 26 * sv, 24 * sv );
            if( busyp <= 160 )
            {
                set_glow( 4 );
                draw_glyph( g, '#', x, y, 14 * sv, 14 * sv );
            }
        }
        else if( g->ENEMIES[ i ].type == 3 )
        {
            // the WALKER: a red 'X' cartwheeling end-over-end as it
            // patrols the rim (wig advances every frame — its angle on
            // the 0..255 turn table)
            set_multiply_color( RGB( 255, 70, 70 ) );
            float rot = ( g->ENEMIES[ i ].wig & 255 ) * 0.024543692;
            draw_rot_glyph( g, 'X', x, y, 22 * sv, 28 * sv, rot );
        }
        else if( g->ENEMIES[ i ].type == 4 )
        {
            // the INCHWORM: a segmented electric body SPANNING the
            // strand -- a head bead at z and the rest trailing behind
            // it. Each bead is projected at its
            // own z on the same lane, so the elongation lies ALONG
            // the web strand automatically (no angle math, no
            // rotation — the projection does the work). The head
            // surges while the body stretches (update_enemies), so
            // the length you SEE is the distance it just MOVED.
            // Electric flicker: each bead strobes white on every
            // 4th frame, phase-shifted per bead — the body ripples.
            // SEGMENTS: one bead per segment left, worm_gap apart
            // behind the head (so a bunched worm is still a visible
            // chain). The whole body shifts colour as it's whittled
            // down: cyan with all three, amber with two, red on its
            // last -- you can read the hits left at a glance.
            Enemy* w = &g->ENEMIES[ i ];
            int body = w->segs >= 3 ? RGB( 80, 230, 255 )
                     : w->segs == 2 ? RGB( 255, 200, 60 ) : RGB( 255, 80, 60 );
            float gap = worm_gap( w );
            int b;
            for( b = 0; b < w->segs; b++ )
            {
                float bz = w->z + gap * b;
                if( bz > 1.0 ) bz = 1.0;             // never past the far cap
                project( g, w->lane, bz );
                float svb = ( g->pscale + ( 1.0 - g->pscale ) * 0.40 ) * 1.15;
                set_multiply_color( ( ( g->frame + i + b ) & 3 ) == 0 ? RGB( 255, 255, 255 ) : body );
                draw_glyph( g, 'W', g->px, g->py, 20 * svb, 16 * svb );
            }
            if( busyp <= 160 )
            {
                set_glow( 4 );
                draw_glyph( g, '*', x, y, 12 * sv, 12 * sv );
            }
        }
        else
        {
            set_multiply_color( RGB( 255, 150, 40 ) );
            draw_glyph( g, 'M', x, y, 22 * sv, 18 * sv );
            if( busyp <= 160 )
            {
                set_glow( 4 );
                draw_glyph( g, 'v', x, y + 10 * sv, 12 * sv, 10 * sv );
            }
        }
        set_blending_mode( BLEND_SOLID );
    }
}

void render_bullets( G* g )
{
    int i;
    set_glow( 0 );
    for( i = 0; i < MAX_BULLETS; i++ )
    {
        if( !g->BULLETS[ i ].alive ) continue;
        project( g, g->BULLETS[ i ].lane, g->BULLETS[ i ].z );
        float s = g->pscale;
        // P2's shots burn CYAN so each player can read their own
        // stream on the shared web (P1 keeps the hot orange)
        if( g->BULLETS[ i ].owner == 1 )
        {
            set_multiply_color( make_color( 140, 220 - (int)( g->BULLETS[ i ].z * 140 ),
                                           255 - (int)( g->BULLETS[ i ].z * 100 ) ) );
        }
        else
        {
            set_multiply_color( make_color( 255, 200 - (int)( g->BULLETS[ i ].z * 180 ),
                                           120 - (int)( g->BULLETS[ i ].z * 100 ) ) );
        }
        // normal shots are 'o' — the old '.' tracer rendered about as
        // small as a background star and vanished in the streaks;
        // rapid-blaster shots stay the fatter '*'
        int bg = 'o';
        float bs = 9 * s + 4;
        if( g->rapid_timer > 0 ) { bg = '*'; bs = 12 * s + 4; }
        draw_glyph( g, bg, g->px, g->py, bs, bs );
    }
    set_blending_mode( BLEND_SOLID );
}

void render_particles( G* g )
{
    int i;
    set_glow( 1 );
    // EVERY-OTHER-PARTICLE PARITY on heavy frames: particles are the
    // #1 draw consumer and the stall culprit. Past 120 alive, each
    // frame draws the complementary half of the swarm — the field
    // reads full density, each spark flickers at 30Hz (an explosion
    // should sparkle anyway) and the worst frame halves its draws.
    int par = 1;
    if( g->partcount > 120 ) par = 2;
    for( i = 0; i < MAX_PARTICLES; i++ )
    {
        Particle* p = &g->PARTICLES[ i ];
        if( !p->alive ) continue;
        if( par == 2 && ( ( i + g->frame ) & 1 ) == 1 ) continue;
        float t = 1.0 - p->life / 50.0;
        if( t < 0 ) t = 0;
        int f = (int)( t * 220 );
        // color varies by glyph: 'o'/'O' particles burn white-hot,
        // the rest go orange-to-red as they age. Sizes twinkle so
        // bursts sparkle instead of just fading.
        int gl = p->glyph;
        if( gl >= 4 )
            set_multiply_color( make_color( 255, 250 - f / 3, 200 - f ) );
        else
            set_multiply_color( make_color( 255, 220 - f, 120 - f / 2 ) );
        float s = 4 + gl * 1.5;
        if( ( i + g->frame ) % 3 == 0 ) s *= 1.4;   // twinkle
        draw_glyph( g, g->GLYPHS[ gl ], p->x, p->y, s, s );
    }
    set_blending_mode( BLEND_SOLID );
}

// shockwave rings: a growing 12-sided polygon of vector bars that
// expands from 6px to ~46px radius while fading from white to deep red
void render_shocks( G* g )
{
    int i; int k;
    set_blending_mode( v32::BlendAlpha );
    for( i = 0; i < MAX_SHOCKS; i++ )
    {
        if( !g->SHOCKS[ i ].alive ) continue;
        float t = g->SHOCKS[ i ].life * ( 1.0 / 22.0 );      // 1 -> 0
        float r = 6 + ( 1.0 - t ) * 40;
        int fade = (int)( t * 160 );
        set_multiply_color( make_color( 255, 100 + fade, 60 + fade / 2 ) );
        float px0 = 0; float py0 = 0;
        float thick = 1.0 + t * 2.0;
        for( k = 0; k <= 12; k++ )
        {
            float a = k * 0.523598776;            // 2*PI/12
            float x = g->SHOCKS[ i ].x + COS32( g, a ) * r;
            float y = g->SHOCKS[ i ].y + SIN32( g, a ) * r;
            // a chord of a circle runs at its midpoint angle plus 90
            // degrees: from point k-1 to point k that's
            // k*30 - 15 + 90 degrees. Known exactly, so draw_segment_c
            // skips the angle search (36 of them per frame with three
            // rings up -- shockwaves come with every explosion)
            if( k > 0 ) draw_segment_c( g, px0, py0, x, y, thick, a + 1.30899694 );
            px0 = x; py0 = y;
        }
    }
    set_blending_mode( BLEND_SOLID );
}

// power-up pods: pulsing 'O' with a glowing '*' core, color-coded by
// type — magenta = superzap, green = AI buddy, gold = extra life
void render_powerups( G* g )
{
    int i;
    for( i = 0; i < MAX_POWERUPS; i++ )
    {
        if( !g->POWERUPS[ i ].alive ) continue;
        project( g, g->POWERUPS[ i ].lane, g->POWERUPS[ i ].z );
        float pulse = 0.8 + 0.25 * sin32( g, g->frame * 0.25 );
        float s = ( 12 * g->pscale + 3 ) * pulse;
        int col;
        if( g->POWERUPS[ i ].type == 0 )      col = RGB( 255, 0, 255 );
        else if( g->POWERUPS[ i ].type == 1 ) col = RGB( 60, 255, 60 );
        else if( g->POWERUPS[ i ].type == 3 ) col = RGB( 80, 220, 255 );
        else if( g->POWERUPS[ i ].type == 4 ) col = RGB( 255, 160, 60 );
        else                                  col = RGB( 255, 220, 60 );
        set_blending_mode( BLEND_SOLID );
        set_multiply_color( col );
        draw_glyph( g, 'O', g->px, g->py, s, s * 0.8 );
        set_glow( 4 );
        draw_glyph( g, '*', g->px, g->py, s * 0.6, s * 0.6 );
        set_blending_mode( BLEND_SOLID );
    }
}

// AI buddy drone: swoops around the web on its OWN lane, bobbing;
// blinks out during its final two seconds
void render_buddy( G* g )
{
    if( g->buddy_timer <= 0 ) return;
    if( g->buddy_timer < 120 && ( g->frame % 8 ) < 3 ) return;
    project( g, g->buddy_lane, 0.10 );
    float fly = fly_factor( g );
    float x = CX + ( g->px - CX ) / fly;
    float y = CY + ( g->py - CY ) / fly;
    y += sin32( g, g->frame * 0.15 ) * 4;
    set_blending_mode( BLEND_SOLID );

    // the buddy is BIOS region 14 — the 10x20 dashed frame — drawn at
    // 22x44 with a ROTATED "ai" inside: the letters are BIOS font
    // regions 'a' and 'i' turned 90 deg clockwise via the ROTOZOOMED
    // command, so the word reads top-to-bottom inside the tall frame
    set_multiply_color( RGB( 120, 255, 160 ) );
    draw_glyph( g, 14, x, y, 22, 44 );

    // letter placement: a rotated draw pivots on the region HOTSPOT
    // (the BIOS font's top-left corner). Scaled (0.8, 0.8), a 10x20
    // glyph covers 8x16; rotated +90 deg (clockwise, y-down screen)
    // that local box lands 16 px LEFT of and 8 px BELOW the anchor —
    // so to center a letter at (x, cy) the anchor goes (x+8, cy-4)
    select_region( 'a' );
    set_drawing_scale( 0.8, 0.8 );
    set_drawing_angle( 1.5707963 );
    draw_region_rotozoomed_at( (int)( x + 8 ), (int)( y - 13 ) );
    select_region( 'i' );
    draw_region_rotozoomed_at( (int)( x + 8 ), (int)( y + 5 ) );
    set_drawing_angle( 0 );
    g->last_region = -1;
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;
    set_blending_mode( BLEND_SOLID );
}
