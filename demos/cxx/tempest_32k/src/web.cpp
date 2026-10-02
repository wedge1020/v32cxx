// *****************************************************************************
//  TEMPEST 32K -- src/web.cpp
//  web geometry: the projection, the sixteen web shapes, rim runs
// *****************************************************************************
#include "tempest.hpp"

// ---------------------------------------------------------------------------
//  Web geometry
// ---------------------------------------------------------------------------
// (lane, z) -> screen point, via g->px / g->py / g->pscale
// level transition geometry factor. Phase A (warp 0->1): the whole
// web, rim included, streams outward past the stationary claw — a
// UNIFORM scale about the tube axis, so geometry never reorders (a
// z-proportional scale let the far end overtake the rim and the tube
// turned inside-out). Phase B (warp 1->2): the NEXT web shrinks back
// in from off-screen to settle under the claw. 1.0 during play.
float fly_factor( G* g )
{
    if( g->warp <= 0 ) return 1.0;
    if( g->warp < 1.0 ) return 1.0 + g->warp * g->warp * 12.0;
    float t = g->warp - 1.0;
    if( t > 1.0 ) t = 1.0;
    return 13.0 - t * 12.0;
}

// The projection's FLY FACTOR -- the level-transition streaming scale
// times the jump camera's pull-back -- depends only on warp and the two
// jump timers, yet project() runs ~200 times a frame. It's cached in g
// and recomputed only when one of those three inputs actually changes.
//
// JUMP CAMERA: while a claw is airborne the camera FOLLOWS it -- the
// whole web (bars, rings, spikes, enemies, pods: everything flows
// through project) RECEDES toward the vanishing point and settles back
// as the claw lands. The profile is a HALF-SINE over the jump's
// progress: 0 at takeoff, peaking mid-jump, 0 at landing. THE SIGN IS
// THE WHOLE TRICK: fly BELOW 1.0 pulls the web inward (the camera rises
// with the jumper); multiplying by (1.0 + arc) swelled the web toward
// the camera instead, exactly backwards. In co-op whichever claw is
// higher drives the pull. Deliberately NOT part of fly_factor itself:
// the spike-sweep crossing test reads fly_factor directly and its
// timing must not shift mid-jump.
//
// (This used to need a "proven expression" spelling -- multiply first,
// divide last -- because `timer / 34.0` silently evaluated as INTEGER
// division. That was a v32c++ codegen bug, fixed in 20261001-dev: the
// natural spelling below is now exactly what runs.)
#define JUMP_FRAMES 34.0
void project_refresh( G* g )
{
    g->proj_warp = g->warp;
    g->proj_jt = g->jump_timer;
    g->proj_jt2 = g->p2_jump_timer;
    float fly = fly_factor( g );
    float jarc = 0;
    if( g->jump_timer > 0 )
        jarc = SIN32( g, 3.14159265 * ( 1.0 - g->jump_timer / JUMP_FRAMES ) );
    if( g->twoplayer && g->p2_jump_timer > 0 )
    {
        float jarc2 = SIN32( g, 3.14159265 * ( 1.0 - g->p2_jump_timer / JUMP_FRAMES ) );
        if( jarc2 > jarc ) jarc = jarc2;
    }
    g->proj_fly = fly * ( 1.0 - jarc * 0.08 );
}

// lanes wrap with a bit mask in project(): that needs a power of two
#if ( LANES & ( LANES - 1 ) ) != 0
#error "LANES must be a power of two (project() wraps lane indices with a mask)"
#endif

void project( G* g, float lane, float z )
{
    if( g->warp != g->proj_warp || g->jump_timer != g->proj_jt ||
        g->p2_jump_timer != g->proj_jt2 )
        project_refresh( g );
    float ang = -1.57079632 + lane * ( 6.28318531 / LANES );
    // interpolate SHAPE across fractional lanes: the player glides
    // (0.12/frame) and rings use i+0.5; a truncated index made the
    // radius step discretely while the angle glided, so the claw
    // visibly jumped off the outline on non-circle levels. Integer
    // lanes hit exact values -- the web bars are unaffected.
    int whole = (int)lane;
    int li = whole & ( LANES - 1 );      // also right for negative lanes
    int ln = ( li + 1 ) & ( LANES - 1 );
    float f = lane - whole;
    float shape = g->SHAPE[ li ] + ( g->SHAPE[ ln ] - g->SHAPE[ li ] ) * f;
    float shapey = g->SHAPE_Y[ li ] + ( g->SHAPE_Y[ ln ] - g->SHAPE_Y[ li ] ) * f;
    // SHAPE scales the HORIZONTAL radius, SHAPE_Y the vertical one.
    // On the classic pass SHAPE_Y is all 1.0 -- the shape reads as a
    // horizontally-modulated ellipse (the tuned look of levels 1-16).
    // On the true-geometry pass SHAPE_Y == SHAPE, so the rim traces
    // the actual polygon in both axes. The fly factor (above) scales
    // everything uniformly, so the tube never inverts.
    float rx = LERP( OUT_RX, IN_RX, z ) * shape * g->proj_fly;
    float ry = LERP( OUT_RY, IN_RY, z ) * shapey * g->proj_fly;
    // camera sway: the vanishing point (z=1) leans toward the player's
    // lane; the rim (z=0) is unaffected. Everything drawn -- web bars,
    // spikes, enemies, pods, bullets -- flows through here, so the
    // whole scene stays internally consistent.
    int ti = TURN_INDEX( ang );
    g->px = CX + g->cam_x * z + g->COS_TABLE[ ti ] * rx;
    g->py = CY + g->cam_y * z + g->SIN_TABLE[ ti ] * ry;
    g->pscale = LERP( 1.0, 0.10, z );
}

// SIXTEEN distinct webs, cycling by level — and TWO passes across the
// 32 levels. The CLASSIC pass (levels 1-16, then every other 16-level
// block) keeps the long-tuned look: the shape factor modulates only
// the horizontal radius, so every web reads as a stylised wide ellipse
// with wavy edges — good, playable, but never really "square". The
// TRUE-GEOMETRY pass (levels 17-32, alternating forever after) runs
// the same sixteen webs with the radius applied to BOTH axes: the
// square is finally a square, the triangle a triangle, the star a real
// 4-point star, the peanut a real peanut, the hinge two real lobes.
// Five kinds are OPEN webs: the rim outline has gaps (CONN[i] = 0), so
// the web cannot be circumnavigated — the claw clamps at gap vertices
// (run_bounds), flippers can't flip across gaps, spikers bounce, and
// the rim/far-cap bars simply aren't drawn over the missing edges.
// Polygon radius trick (square/triangle) — the EXACT regular-n-gon
// outline, because mild cos-shaped approximations read as wobbly
// circles at 16 lanes: square r = 1/(|cos t| + |sin t|) has genuinely
// FLAT sides (the sampled lanes lie ON the sides, so the chord
// outline IS the square); triangle folds the angle into one
// 120-degree wedge and takes r = 0.5/cos(u - 60 deg), whose deep 0.5
// mid-sides give it a real triangular read.
void make_shape( G* g )
{
    int i;
    int kind = ( g->level - 1 ) % 16;
    // the web bar angle caches (RIM_ANG/CAP_ANG) are keyed to this
    // shape — invalidate so the first render_web of the level refills
    g->web_ang_ok = 0;
    // TRUE-GEOMETRY pass on levels 17-32 (and every other 16-level
    // block forever): SHAPE_Y == SHAPE, so both axes carry the shape
    int true2d = ( ( ( g->level - 1 ) / 16 ) % 2 );
    for( i = 0; i < LANES; i++ ) g->CONN[ i ] = 1;
    if( kind == 4 ) { g->CONN[ 0 ] = 0; g->CONN[ 8 ] = 0; }   // OPEN: split arcs
    if( kind == 7 ) g->CONN[ 4 ] = 0;                         // OPEN: square doorway
    if( kind == 10 ) { g->CONN[ 2 ] = 0; g->CONN[ 9 ] = 0; }  // OPEN: hinge arms
    if( kind == 12 ) g->CONN[ 3 ] = 0;                        // OPEN: split curly
    if( kind == 15 ) { g->CONN[ 0 ] = 0; g->CONN[ 5 ] = 0; g->CONN[ 10 ] = 0; } // OPEN: 3 arcs
    for( i = 0; i < LANES; i++ )
    {
        float w = 1.0;
        float t = i * 0.392699081;   // i * PI/8
        if( kind == 0 ) w = 1.0;                                    // circle
        if( kind == 1 ) w = 0.72 + 0.38 * cos32( g, t * 4 );        // rounded square
        if( kind == 2 ) w = 0.65 + 0.45 * fabs_sin( g, t * 2 );    // star
        if( kind == 3 ) w = 0.85 + 0.3 * cos32( g, t * 8 );         // flower
        if( kind == 4 ) w = 0.95 + 0.1 * cos32( g, t * 2 );         // open: two arcs
        if( kind == 5 ) w = 0.88 + 0.28 * cos32( g, t * 3 );        // trefoil wave
        if( kind == 6 ) w = 0.55 + 0.5 * fabs_sin( g, t );          // peanut lobes
        if( kind == 7 )   // open: tilted square — TRUE square radius,
        {                 // rotated 22.5 deg: vertices at lanes 3/7/11/15
            float ca = fabs_sin( g, t + 1.9634954 );   // |cos(t+pi/8)|
            float sb = fabs_sin( g, t + 0.3926991 );   // |sin(t+pi/8)|
            w = 1.0 / ( ca + sb );
        }
        if( kind == 8 )   // square: TRUE radius 1/(|cos t|+|sin t|) —
        {                 // vertices at lanes 0/4/8/12, flat sides
            float ca = fabs_sin( g, t + 1.5707963 );   // |cos t|
            float sb = fabs_sin( g, t );               // |sin t|
            w = 1.0 / ( ca + sb );
        }
        if( kind == 9 )   // triangle: fold to one 120-degree wedge,
        {                 // vertices at 0/120/240 degrees — EXACT radius
            float u = t;
            while( u >= 2.0943951 ) u -= 2.0943951;   // mod 120 deg
            w = 0.5 / cos32( g, u - 1.0471976 );
        }
        if( kind == 10 ) w = 0.68 + 0.32 * cos32( g, t * 2 - 4.7123890 ); // open: hinge
        if( kind == 11 ) w = 0.76 + 0.14 * cos32( g, t * 3 )
                                  + 0.10 * cos32( g, t * 5 + 0.9 );  // curly-cue
        if( kind == 12 ) w = 0.76 + 0.14 * cos32( g, t * 3 )
                                  + 0.10 * cos32( g, t * 5 + 0.9 );  // open: split curly
        if( kind == 13 ) w = 0.78 + 0.16 * cos32( g, t )
                                  + 0.06 * cos32( g, t * 2 );        // egg (lopsided)
        if( kind == 14 ) w = 0.68 + 0.32 * fabs_sin( g, t * 3 + 0.3926991 ); // saw star
        if( kind == 15 ) w = 1.0;                                    // open: 3 arcs (ring)
        g->SHAPE[ i ] = w;
        if( true2d ) g->SHAPE_Y[ i ] = w;
        else         g->SHAPE_Y[ i ] = 1.0;
    }
}

// 1 = the rim is fully connected (no gaps): movement is unrestricted
// and pods may spawn in any lane. MUST be checked before run_bounds —
// on a closed web its walks wrap all the way around and come back as
// a bogus "single-lane run" (that bug froze the claw at integer lanes).
int web_full( G* g )
{
    int i;
    for( i = 0; i < LANES; i++ ) if( !g->CONN[ i ] ) return 0;
    return 1;
}

// contiguous rim run containing lane li: walks outward over existing
// edges (circularly — a run may wrap the lane 15 -> 0 seam). *lo..*hi
// is the run; lo > hi means the run wraps.
void run_bounds( G* g, int li, int* lo, int* hi )
{
    int a = li; int b = li; int k;
    for( k = 0; k < LANES - 1; k++ )
    {
        int prev = a - 1; if( prev < 0 ) prev += LANES;
        if( !g->CONN[ prev ] ) break;
        a = prev;
    }
    for( k = 0; k < LANES - 1; k++ )
    {
        if( !g->CONN[ b ] ) break;
        b += 1;
        if( b >= LANES ) b -= LANES;
    }
    *lo = a; *hi = b;
}
