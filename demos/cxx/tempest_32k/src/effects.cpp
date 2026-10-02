// *****************************************************************************
//  TEMPEST 32K -- src/effects.cpp
//  shockwave, particle and starfield updates; the starfield renderer
// *****************************************************************************
#include "tempest.hpp"

void update_shocks( G* g )
{
    int i;
    for( i = 0; i < MAX_SHOCKS; i++ )
    {
        if( !g->SHOCKS[ i ].alive ) continue;
        g->SHOCKS[ i ].life--;
        if( g->SHOCKS[ i ].life <= 0 ) g->SHOCKS[ i ].alive = 0;
    }
}

// POINTER PER ELEMENT: the Vircon32 C compiler recomputes the full
// address of g->PARTICLES[ i ].field (load g, add the array offset,
// multiply i by the struct size, add the field) on EVERY access -- six
// instructions, eight accesses per live particle. Taking the element's
// address once makes each access a single offset load. The same
// pattern is used in the other hot per-element loops.
//
// Also keeps g->partcount (live particles, read by the render-phase
// throttles) current: this loop already visits every particle, so the
// separate once-a-frame count_particles walk is no longer needed.
void update_particles( G* g )
{
    int i;
    int alive = 0;
    for( i = 0; i < MAX_PARTICLES; i++ )
    {
        Particle* p = &g->PARTICLES[ i ];
        if( !p->alive ) continue;
        p->x += p->vx;
        p->y += p->vy;
        p->vx *= 0.96;
        p->vy *= 0.96;
        p->life--;
        if( p->life <= 0 ) p->alive = 0;
        else alive++;
    }
    g->partcount = alive;
}

// ---------------------------------------------------------------------------
//  Background starfield — bright, cheap, always on
// ---------------------------------------------------------------------------
void init_starfield( G* g )
{
    int i;
    for( i = 0; i < MAX_STARS; i++ )
    {
        g->STARFIELD[ i ].x = frand( g ) * 640;
        g->STARFIELD[ i ].y = frand( g ) * 336;
        g->STARFIELD[ i ].spd = 0.8 + frand( g ) * 1.6;
        // seed the direction cache (cam starts centred)
        float dx = g->STARFIELD[ i ].x - CX;
        float dy = g->STARFIELD[ i ].y - CY;
        float d = sqrt( dx * dx + dy * dy );
        if( d < 1 ) d = 1;
        g->STARFIELD[ i ].dirx = dx / d;
        g->STARFIELD[ i ].diry = dy / d;
    }
}

void update_stars( G* g )
{
    int i;
    // stars streak only while the old web flies OUT (phase A); during
    // the new web's fly-in they calm back down
    float boost = 1.0;
    if( g->warp > 0 && g->warp < 1.0 ) boost = 1.0 + g->warp * 5.0;
    // the stream radiates from the web's FAR END — the same swaying
    // vanishing point project() uses at z=1 (CX + cam_x, CY + cam_y).
    // Streaming from the fixed screen centre instead looked detached
    // from the tunnel whenever the camera leaned toward the player.
    float ox = CX + g->cam_x;
    float oy = CY + g->cam_y;
    for( i = 0; i < MAX_STARS; i++ )
    {
        Star* st = &g->STARFIELD[ i ];   // see update_particles
        // DIRECTION CACHE: a star's unit direction from the vanishing
        // point only changes when the camera lean moves (a slow ease),
        // so each star refreshes its cached vector one frame in eight
        // (staggered by index) and the other seven use the cache.
        // This deletes 80 sqrt calls and 160 divisions per frame --
        // the bulk of the star update's CPU cost.
        if( ( ( i + g->frame ) & 7 ) == 0 )
        {
            float dx = st->x - ox;
            float dy = st->y - oy;
            float d = sqrt( dx * dx + dy * dy );
            if( d < 1 ) d = 1;
            st->dirx = dx / d;
            st->diry = dy / d;
        }
        float step = st->spd * boost;
        st->x += st->dirx * step;
        st->y += st->diry * step;
        if( st->x < -4 || st->x > 644 || st->y < -4 || st->y > 340 )
        {
            // respawn near the vanishing point so the stream is
            // endless -- the spawn angle IS the direction: no sqrt
            float a = frand( g ) * 6.28318;
            float r = 4 + frand( g ) * 30;
            float ca = COS32( g, a );
            float sa = SIN32( g, a );
            st->x = ox + ca * r;
            st->y = oy + sa * r;
            st->dirx = ca;
            st->diry = sa;
            st->spd = 0.8 + frand( g ) * 1.6;
        }
    }
}

void render_starfield( G* g )
{
    int i;
    set_blending_mode( BLEND_SOLID );
    // EVERY-OTHER-STAR PARITY: each frame draws half the stars, the
    // complementary half next frame — the FIELD reads full density
    // while the per-frame draw count halves, and each dot's 30Hz
    // on/off reads as twinkle. On busy frames (explosion salvos)
    // thin to a quarter.
    int starstep = 1;
    if( g->partcount > 120 ) starstep = 2;
    // the two star colors, built once instead of once per star
    int near_color = RGB( 240, 245, 255 );
    int far_color = RGB( 150, 165, 215 );
    for( i = 0; i < MAX_STARS; i += starstep )
    {
        if( ( ( i + g->frame ) / starstep ) % 2 == 1 ) continue;
        Star* st = &g->STARFIELD[ i ];
        // fast/near stars: big and white; slow/far: smaller, cool blue
        if( st->spd > 1.6 )
        {
            set_multiply_color( near_color );
            draw_glyph( g, '.', st->x, st->y, 5, 5 );
        }
        else
        {
            set_multiply_color( far_color );
            draw_glyph( g, '.', st->x, st->y, 3, 3 );
        }
    }
}
