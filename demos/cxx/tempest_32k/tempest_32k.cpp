// *****************************************************************************
//  TEMPEST 32K  —  a Tempest 2000-style tunnel shooter for Vircon32,
//  written in the v32c++ subset, rendered entirely with ASCII glyphs from
//  the BIOS font texture (-1). No custom textures, no 3D hardware.
//
//  Build:  v32c++ -I . tempest32k.cpp -o tempest32k.c
//
//  Fake-3D technique:
//    * The web is N lanes around a center point. A point is (lane, z in 0..1).
//      We lerp the radius between an outer (near) ellipse and an inner (far)
//      ellipse as z grows — the "flying into the tunnel" perspective without
//      any matrix math. Glyph size lerps the same way; that alone sells depth.
//    * Tunnel edges / depth rings are dotted chains of scaled '.' glyphs.
//    * All glow (particles, bullets, party text) uses ADD blending + hue
//      cycling — the whole Tempest-2000 look.
//
//  IMPORTANT Vircon32 C limitation worked around here (twice over):
//    (1) Vircon32 C rejects assigning ANY const-qualified value into a
//        plain one ("discards const qualifier") — stricter than gcc and
//        than the C standard. So reading a `const` global on the RHS of
//        an assignment is a hard error. And (2) file-scope variables are
//        read-only (ROM) unless declared with Vircon32's `global`
//        keyword, which v32c++ cannot emit — so mutable statics are out
//        too.
//    Consequences for this file:
//    * NO `const` variables anywhere — all tunables are #define macros.
//      The v32c++ lexer carries `#` lines verbatim to the top of the
//      generated C, and Vircon32 C's own preprocessor expands them
//      (its SDK headers rely on #define, e.g. color_black).
//    * ALL mutable state lives in a `struct G`, heap-allocated with
//      `new` (real malloc() via the generated v32_new_G runtime), and
//      a `G*` pointer is threaded through every function. Stack and
//      heap are the only writable memory.
//
//  Other Vircon32 API notes (checked against the SDK headers):
//    * No `color` type: colors are plain ints, ABGR word order, red low byte.
//    * set_multiply_color(int); set_blending_mode(int) — 0 = solid,
//      v32::BlendAlpha = 0x20, v32::BlendAdd = 0x21.
//    * BIOS font: texture -1 has ONE REGION PER CHARACTER — select_region
//      with the character code, then draw_region_zoomed_at (the plain
//      DrawRegion command ignores the scale ports). Base glyph is 10x20 px.
//    * Array sizes must be INT LITERALS (grammar rule), so LANES = 16 is
//      spelled out in every array declaration.
//
//  v32c++ subset compliance:
//    * No STL, no templates, no ternary, no array initializer lists — all
//      arrays are filled at runtime.
//    * `main` is void (transpiler emits `void main(void)`).
// *****************************************************************************

#include <v32/video.hpp>
#include <v32/input.hpp>
#include <v32/time.hpp>
#include <v32/math.hpp>     // sqrt() for segment lengths (hardware pow)

// 0 = solid (no blending); the v32 enum only names alpha/add/subtract
#define BLEND_SOLID 0

// ---------------------------------------------------------------------------
//  Cart metadata (parsed by the v32c++ lexer; delete if your build rejects)
// ---------------------------------------------------------------------------
#title "TEMPEST 32K"
#version 0.2

// ---------------------------------------------------------------------------
//  Tunables — #define, NOT const variables: reading a const on the RHS of
//  an assignment is a hard error in Vircon32 C ("discards const qualifier")
// ---------------------------------------------------------------------------
#define LANES 16          // web lanes (array sizes below spell this as 16!)
#define RINGS 7
#define MAX_BULLETS 32
#define MAX_ENEMIES 24
#define MAX_PARTICLES 220
#define MAX_STARS 40

#define CX 320
#define CY 168
#define OUT_RX 300
#define OUT_RY 158
#define IN_RX 34
#define IN_RY 18

// CAMERA SWAY (Tempest 2000 flavour): the tube's vanishing point leans
// toward the player's lane, eased over ~20 frames. 0 disables. The
// rim (z=0) stays FIXED — only mid-tunnel and far geometry shifts, so
// gameplay readability and the hard-won bar alignment are untouched.
// 0.18 * the claw's offset-from-axis = up to ~54px of lean.
#define CAM_SWAY 0.18
#define CAM_EASE 0.05

// A/B KILL-SWITCHES for the additive-glow white-out bug, per layer.
// BISECTION RESULT: BlendAdd (0x21) white-screens the real emulator
// in this game no matter which layer uses it; BlendAlpha (0x20) works
// perfectly. A single static unscaled glyph does NOT reproduce, so
// the Add bug needs varying scale / color / position (Zoomed path?).
// DEFAULT: alpha glow everywhere.
#define GLOW_MODE  2   // 0 = solid, 1 = BlendAdd (0x21), 2 = BlendAlpha (0x20)
#define GLOW_ALL   1
#define GLOW_BULLET 0   // bullets
#define GLOW_PART   0   // particles
#define GLOW_SPIKE  0   // spikes
#define GLOW_CLAW   0   // player claw accents
#define GLOW_ENEMY  0   // enemy cores

// ---------------------------------------------------------------------------
//  ALL mutable game state lives here, on the heap (see header comment)
// ---------------------------------------------------------------------------
struct Bullet
{
    int alive;
    float lane;
    float z;
};

struct Enemy
{
    int alive;
    int type;          // 0 = flipper, 1 = tanker, 2 = spiker
    float lane;
    float z;
    int cooldown;
    int wig;
};

struct Particle
{
    int alive;
    float x; float y;
    float vx; float vy;
    int life;
    int glyph;
};

struct Shock
{
    int alive;
    float x; float y;
    int life;
};

// background starfield: screen-space stars drifting outward from the
// vanishing point (matches the fly-into-the-tunnel camera), streaking
// during warp-out. Deliberately mild — dim dots, low speed.
struct Star
{
    float x; float y;
    float spd;               // radial px/frame
};

struct PowerUp
{
    int alive;
    int type;        // 0 = superzap refill, 1 = AI buddy, 2 = extra life
    float lane;
    float z;
};

struct G
{
    // math tables
    float SIN_TABLE[ 256 ];
    float COS_TABLE[ 256 ];

    // web geometry
    float SHAPE[ 16 ];
    float px; float py; float pscale;   // project() outputs

    // entities
    char  GLYPHS[ 6 ];
    Bullet   BULLETS[ 32 ];
    Enemy    ENEMIES[ 24 ];
    Particle PARTICLES[ 220 ];
    Shock    SHOCKS[ 3 ];
    PowerUp  POWERUPS[ 4 ];
    // array size MUST be an int literal (v32c++ grammar rule — macros
    // like MAX_STARS are fine in expressions but NOT in declarations;
    // same reason LANES is spelled 16 everywhere below)
    Star     STARFIELD[ 40 ];
    float SPIKE[ 16 ];

    // game state
    float player_lane;
    float cam_x; float cam_y;   // camera sway: vanishing-point offset
    int   lives;
    int   score;
    int   level;
    int   superzaps;
    int   spawn_timer;
    int   spawn_interval;
    int   state;               // 0 play, 1 dying, 2 warp-out, 3 game over
    int   state_timer;
    char  message[ 24 ];
    int   message_timer;
    float warp;
    int   launched;
    int   fire_cooldown;
    int   jump_timer;       // claw leap: frames airborne (34 total)
    int   jump_cooldown;    // frames until the next leap is allowed
    int   buddy_timer;      // AI buddy drone: frames remaining
    int   buddy_cooldown;   // AI buddy: frames until next auto-shot
    int   powerup_timer;    // frames until the next power-up spawns
    int   frame;
    int   rng_state;

    // draw-call throttles: the last region/scale we set, so draw_glyph
    // can skip redundant GPU register writes (a big deal at hundreds of
    // glyphs per frame)
    int   last_region;
    float last_scale_x;
    float last_scale_y;
};

// ---------------------------------------------------------------------------
//  Math helpers
// ---------------------------------------------------------------------------
float lerp( float a, float b, float t )
{
    return a + ( b - a ) * t;
}

float cos_taylor( float x );   // fwd: used by build_tables below

float sin_taylor( float x )
{
    // wrap to [-pi, pi] first
    while( x > 3.14159265 ) x -= 6.28318531;
    while( x < -3.14159265 ) x += 6.28318531;
    // THEN fold into [-pi/2, pi/2] via symmetry: the 4-term series is
    // excellent there (error ~2e-5) but GARBAGE near +/-pi, where it
    // gave sin(pi) = -0.075 instead of 0. That corrupted the tables
    // around index 128 (sin, due west) and 64 (cos, due south), which
    // tilted every west/south-pointing bar ~4.3 degrees — the east
    // spokes visibly missing the far cap, the bottom juts, misaligned
    // vertex endings. sin(x) = sin(pi - x) for x in (pi/2, pi];
    // sin(x) = sin(-pi - x) for x in [-pi, -pi/2).
    if( x > 1.57079632 )   x = 3.14159265 - x;
    if( x < -1.57079632 )  x = -3.14159265 - x;
    float x2 = x * x;
    return x * ( 1.0
          - x2 / 6.0
          + x2 * x2 / 120.0
          - x2 * x2 * x2 / 5040.0 );
}

void build_tables( G* g )
{
    int i;
    for( i = 0; i < 256; i++ )
    {
        float a = ( i * 3.14159265 * 2.0 ) / 256.0;
        g->SIN_TABLE[ i ] = sin_taylor( a );
        g->COS_TABLE[ i ] = cos_taylor( a );
    }
}

// Table index: ROUND to nearest entry, not truncate. (int) truncates
// toward zero, so negative angles landed 1 entry off (lane 0 vs lane
// 16 differed by 1.4 deg -> the top seam gap), and products landing
// near an integer boundary (lanes 5, 8) jittered by +-1 entry. The
// +1024 (four full turns) keeps the sum positive for every angle the
// game uses (|x| < 25 rad), so truncation direction never matters,
// and +0.5 rounds to nearest. 15.9999 and 16.0001 both give 16.
float sin32( G* g, float x )
{
    int i = ( (int)( x * 40.7436611 + 1024.5 ) ) & 255;
    return g->SIN_TABLE[ i ];
}

float cos_taylor( float x )
{
    return sin_taylor( x + 1.57079632 );
}

float cos32( G* g, float x )
{
    int i = ( (int)( x * 40.7436611 + 1024.5 ) ) & 255;
    return g->COS_TABLE[ i ];
}

float fabs_sin( G* g, float x )
{
    float s = sin32( g, x );
    if( s < 0 ) s = -s;
    return s;
}

int rng( G* g )
{
    g->rng_state = g->rng_state * 1103515245 + 12345;
    return ( g->rng_state >> 16 ) & 0x7FFF;
}

float frand( G* g )
{
    return ( rng( g ) % 1000 ) * 0.001;
}

// ---------------------------------------------------------------------------
//  Color helpers — colors are ints, ABGR word order (red = low byte)
// ---------------------------------------------------------------------------
int make_color( int r, int g_, int b )
{
    return ( 255 << 24 ) | ( b << 16 ) | ( g_ << 8 ) | r;
}

int hue( int h )
{
    int region = ( h / 43 ) % 6;
    int f = ( h % 43 ) * 6;
    if( region == 0 ) return make_color( 255, f, 0 );
    if( region == 1 ) return make_color( 255 - f, 255, 0 );
    if( region == 2 ) return make_color( 0, 255, f );
    if( region == 3 ) return make_color( 0, 255 - f, 255 );
    if( region == 4 ) return make_color( f, 0, 255 );
    return make_color( 255, 0, 255 - f );
}

// All "glow" drawing routes through here, gated per layer by the
// GLOW_* switches; GLOW_MODE picks the blending used by enabled
// layers: 0 = solid, 1 = Add, 2 = Alpha. (Alpha blending is the
// fallback glow if the emulator's Add path proves broken.)
void set_glow( int layer )
{
    int on = GLOW_ALL;
    if( layer == 0 ) { if( GLOW_BULLET ) on = 1; }
    if( layer == 1 ) { if( GLOW_PART )   on = 1; }
    if( layer == 2 ) { if( GLOW_SPIKE )  on = 1; }
    if( layer == 3 ) { if( GLOW_CLAW )   on = 1; }
    if( layer == 4 ) { if( GLOW_ENEMY )  on = 1; }
    if( !on || GLOW_MODE == 0 ) set_blending_mode( BLEND_SOLID );
    else if( GLOW_MODE == 1 ) set_blending_mode( v32::BlendAdd );
    else set_blending_mode( v32::BlendAlpha );
}

// ---------------------------------------------------------------------------
//  Glyph renderer — the ONLY draw primitive in the whole game
// ---------------------------------------------------------------------------
// Draws BIOS font character c centered at (x,y), sized w*h pixels.
// Base glyph is 10x20 px, so scale factors are (w/10, h/20). Caches the
// last region + scale in g to skip redundant GPU register writes —
// a big deal at hundreds of glyphs per frame.
void draw_glyph( G* g, int c, float x, float y, float w, float h )
{
    if( g->last_region != c )
    {
        select_region( c );
        g->last_region = c;
    }
    float sx = w / 10.0;
    float sy = h / 20.0;
    if( g->last_scale_x != sx || g->last_scale_y != sy )
    {
        set_drawing_scale( sx, sy );
        g->last_scale_x = sx;
        g->last_scale_y = sy;
    }
    // ZOOMED command, not plain DrawRegion: the GPU command table
    // shows plain DrawRegion ignores the scale ports — only the
    // Zoomed/Rotozoomed variants apply them
    draw_region_zoomed_at( (int)( x - w / 2 ), (int)( y - h / 2 ) );
}

// dotted line of tiny glyphs between two points (tunnel edges, rings).
// HARD CAP on dots: the Vircon32 GPU fits a limited number of draw
// calls per frame -- going over stalls the emulator (100% CPU) and the
// additive glow accumulates into a white-out. 24 dots max.
void draw_dot_line( G* g, float x0, float y0, float x1, float y1, int step )
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = dx * dx + dy * dy;
    int n = (int)( len / ( step * step ) );
    if( n < 1 ) n = 1;
    if( n > 24 ) n = 24;
    int i;
    for( i = 0; i <= n; i++ )
    {
        float t = ( i * 1.0 ) / n;
        // alternate glyph sizes for a shimmer
        float s = 2.0;
        if( ( i + g->frame ) % 2 == 0 ) s = 3.0;
        draw_glyph( g, '.', x0 + dx * t, y0 + dy * t, s, s );
    }
}

int string_len( char* s )
{
    int n = 0;
    while( s[ n ] != 0 ) n++;
    return n;
}

// ---------------------------------------------------------------------------
//  Vector-style web outlines
// ---------------------------------------------------------------------------

//  Best-match angle for direction (dx,dy), as a 256-step turn index:
//  coarse 32-step scan of the direction tables, then a +-4 refine
//  around the winner. Multiply by 2*PI/256 (0.024543692) for radians.
//  Convention matches set_drawing_angle: in y-down screen coords the
//  resulting angle grows clockwise, which is what the GPU expects.
int dir_angle( G* g, float dx, float dy )
{
    int i;
    int best = 0;
    float bestdot = -1000000000.0;
    for( i = 0; i < 256; i += 8 )          // coarse: 32 steps
    {
        float dot = g->COS_TABLE[ i ] * dx + g->SIN_TABLE[ i ] * dy;
        if( dot > bestdot ) { bestdot = dot; best = i; }
    }
    int j0 = best - 4;
    if( j0 < 0 ) j0 += 256;
    for( i = 0; i < 8; i++ )               // refine around the winner
    {
        int j = ( j0 + i ) & 255;
        float dot = g->COS_TABLE[ j ] * dx + g->SIN_TABLE[ j ] * dy;
        if( dot > bestdot ) { bestdot = dot; best = j; }
    }
    return best;   // 256-step turn index; caller converts to radians
}

//  Solid bar from (x0,y0) to (x1,y1), thickness in pixels. BIOS font
//  REGION 20 is a solid 10x20 block with its DEFAULT hotspot at the
//  TOP-LEFT (print_at draws text top-left referenced, so all BIOS
//  regions are defined that way). NEVER touch region 20's bounds or
//  hotspot: redefining them (define_region / set_region_hotspot)
//  replaces the BIOS's correct geometry with guessed coordinates and
//  the draws sample the texture's magenta region-outline lines
//  instead of the solid block (empirically confirmed).
//
//  Placement with a top-left hotspot: rotation pivots at the draw
//  point, so we draw at (start + half-thickness perpendicular offset),
//  which centers the bar on the line. Angle in RADIANS (video.h).
//
//  SOLID-alpha only — additive bars would re-trigger the emulator's
//  BlendAdd saturation bug. Invalidates draw_glyph's region/scale
//  cache, since this bypasses it and changes GPU state directly.
void draw_segment( G* g, float x0, float y0, float x1, float y1, float thick )
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrt( dx * dx + dy * dy );
    if( len < 2.0 ) return;

    float idx = dir_angle( g, dx, dy );   // integer turn index
    // SUB-INDEX REFINEMENT: the perpendicular component of (dx,dy)
    // relative to the table direction equals sin(angle error), which
    // for our small residual (<0.7 deg) is the error in radians.
    // Adding it gives a near-exact angle, killing vertex drift.
    float c0 = g->COS_TABLE[ (int)idx ];
    float s0 = g->SIN_TABLE[ (int)idx ];
    float a = idx * 0.024543692 + ( -s0 * dx + c0 * dy ) / len;
    float c = cos32( g, a );
    float s = sin32( g, a );

    select_region( 20 );   // solid block, DEFAULT hotspot (top-left)
    set_drawing_scale( len / 10.0 + 0.3, thick / 20.0 );
    set_drawing_angle( a );
    // bar extends from the draw point along (c,s) for `len` and
    // perpendicular for `thick`; offsetting the draw point by half the
    // thickness along the perpendicular centers the bar on the line
    draw_region_rotozoomed_at( (int)( x0 + s * thick * 0.5 ),
                               (int)( y0 - c * thick * 0.5 ) );
    set_drawing_angle( 0 );
    g->last_region = -1;   // draw_glyph cache is stale now
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;
}

void draw_party_text( G* g, char* s, float x, float y, float size, int huebase )
{
    int i = 0;
    while( s[ i ] != 0 )
    {
        float wob = sin32( g, i * 0.35 + g->frame * 0.05 ) * size * 0.28;
        set_multiply_color( hue( huebase + i * 9 + ( g->frame >> 1 ) ) );
        draw_glyph( g, s[ i ],
                    x + ( i - string_len( s ) / 2.0 ) * size * 0.62,
                    y + wob,
                    size, size * 1.15 );
        i++;
    }
}

void draw_text( G* g, char* s, float x, float y, float size, int c )
{
    int i = 0;
    while( s[ i ] != 0 )
    {
        set_multiply_color( c );
        draw_glyph( g, s[ i ], x + i * size * 0.62, y, size, size );
        i++;
    }
}

// ---------------------------------------------------------------------------
//  Web geometry
// ---------------------------------------------------------------------------
// (lane, z) -> screen point, via g->px / g->py / g->pscale
void project( G* g, float lane, float z )
{
    float ang = -1.57079632 + ( lane / LANES ) * 6.28318531;
    // interpolate SHAPE across fractional lanes: the player glides
    // (0.09/frame) and rings use i+0.5; a truncated index made the
    // radius step discretely while the angle glided, so the claw
    // visibly jumped off the outline on non-circle levels. Integer
    // lanes hit exact values — the web bars are unaffected.
    int li = ( (int)lane ) % LANES;
    if( li < 0 ) li += LANES;
    int ln = ( li + 1 ) % LANES;
    float f = lane - (int)lane;
    float shape = g->SHAPE[ li ] + ( g->SHAPE[ ln ] - g->SHAPE[ li ] ) * f;
    float rx = lerp( OUT_RX, IN_RX, z ) * shape;
    float ry = lerp( OUT_RY, IN_RY, z );
    // level transition ("the web flies past you"): during warp-out the
    // deeper geometry streams outward past the stationary claw. The
    // rim (z=0) is anchored — the claw, rim outline and spikes tips'
    // foot stay put; spokes stretch, rings/far cap rush off-screen.
    float fly = 1.0 + g->warp * g->warp * 7.0 * z;
    rx *= fly;
    ry *= fly;
    // camera sway: the vanishing point (z=1) leans toward the player's
    // lane; the rim (z=0) is unaffected. Everything drawn — web bars,
    // spikes, enemies, pods, bullets — flows through here, so the whole
    // scene stays internally consistent.
    float cx = CX + g->cam_x * z;
    float cy = CY + g->cam_y * z;
    g->px = cx + cos32( g, ang ) * rx;
    g->py = cy + sin32( g, ang ) * ry;
    g->pscale = lerp( 1.0, 0.10, z );
}

void make_shape( G* g )
{
    int i;
    for( i = 0; i < LANES; i++ )
    {
        float w = 1.0;
        float t = i * 0.392699081;   // i * PI/8
        if( g->level % 4 == 1 ) w = 1.0;                                    // circle
        if( g->level % 4 == 2 ) w = 0.72 + 0.38 * cos32( g, t * 4 );        // rounded square
        if( g->level % 4 == 3 ) w = 0.65 + 0.45 * fabs_sin( g, t * 2 + g->level ); // star
        if( g->level % 4 == 0 ) w = 0.85 + 0.3 * cos32( g, t * 8 );         // flower
        g->SHAPE[ i ] = w;
    }
}

// ---------------------------------------------------------------------------
//  Spawning / effects
// ---------------------------------------------------------------------------
void show_message( G* g, char* s )
{
    int i = 0;
    while( s[ i ] != 0 && i < 23 ) { g->message[ i ] = s[ i ]; i++; }
    g->message[ i ] = 0;
    g->message_timer = 130;
}

void spawn_enemy( G* g, int type )
{
    int i;
    for( i = 0; i < MAX_ENEMIES; i++ )
    {
        if( !g->ENEMIES[ i ].alive )
        {
            g->ENEMIES[ i ].alive = 1;
            g->ENEMIES[ i ].type = type;
            g->ENEMIES[ i ].lane = rng( g ) % LANES;
            if( type == 2 ) g->ENEMIES[ i ].z = 0.45;   // spiker: mid-tunnel
            else            g->ENEMIES[ i ].z = 1.0;
            g->ENEMIES[ i ].cooldown = 30 + rng( g ) % 40;
            g->ENEMIES[ i ].wig = rng( g ) % 256;
            return;
        }
    }
}

void spawn_near( G* g, float lane, float z )
{
    int i;
    for( i = 0; i < MAX_ENEMIES; i++ )
    {
        if( !g->ENEMIES[ i ].alive )
        {
            g->ENEMIES[ i ].alive = 1;
            g->ENEMIES[ i ].type = 0;
            g->ENEMIES[ i ].lane = lane;
            if( g->ENEMIES[ i ].lane >= LANES ) g->ENEMIES[ i ].lane -= LANES;
            g->ENEMIES[ i ].z = z;
            g->ENEMIES[ i ].cooldown = 20;
            g->ENEMIES[ i ].wig = rng( g ) % 256;
            return;
        }
    }
}

// live enemy count: the old enemies_alive counter drifted (tanker
// splits spawned an enemy without counting it, so the counter went
// negative and the level NEVER registered as clear — spawning locked
// up and the player hit an enemy drought). Counting the real array
// can't drift.
int count_enemies( G* g )
{
    int i; int n = 0;
    for( i = 0; i < MAX_ENEMIES; i++ ) if( g->ENEMIES[ i ].alive ) n++;
    return n;
}

void burst( G* g, float x, float y, int count, int strength )
{
    int n = 0;
    int i;
    for( i = 0; i < MAX_PARTICLES && n < count; i++ )
    {
        if( !g->PARTICLES[ i ].alive )
        {
            g->PARTICLES[ i ].alive = 1;
            g->PARTICLES[ i ].x = x;
            g->PARTICLES[ i ].y = y;
            float a = frand( g ) * 6.28318531;
            float sp = ( 0.4 + frand( g ) ) * strength;
            g->PARTICLES[ i ].vx = cos32( g, a ) * sp;
            g->PARTICLES[ i ].vy = sin32( g, a ) * sp;
            g->PARTICLES[ i ].life = 20 + rng( g ) % 30;
            g->PARTICLES[ i ].glyph = rng( g ) % 6;
            n++;
        }
    }
}

// expanding shockwave ring: 3 slots, each renders as a growing 12-sided
// polygon of vector bars that fades over ~22 frames
void shockwave( G* g, float x, float y )
{
    int s;
    for( s = 0; s < 3; s++ )
    {
        if( !g->SHOCKS[ s ].alive )
        {
            g->SHOCKS[ s ].alive = 1;
            g->SHOCKS[ s ].x = x;
            g->SHOCKS[ s ].y = y;
            g->SHOCKS[ s ].life = 22;
            return;
        }
    }
}

// big juicy death: particle burst + expanding shockwave ring
void explode( G* g, float x, float y, int count, int strength )
{
    burst( g, x, y, count, strength );
    shockwave( g, x, y );
}

// ---------------------------------------------------------------------------
//  Input / update
// ---------------------------------------------------------------------------
void kill_player( G* g );
void level_clear( G* g );

void fire( G* g )
{
    if( g->fire_cooldown > 0 ) return;
    g->fire_cooldown = 6;
    int i;
    for( i = 0; i < MAX_BULLETS; i++ )
    {
        if( !g->BULLETS[ i ].alive )
        {
            g->BULLETS[ i ].alive = 1;
            g->BULLETS[ i ].lane = g->player_lane;
            g->BULLETS[ i ].z = 0.02;
            return;
        }
    }
}

void superzap( G* g )
{
    if( g->superzaps <= 0 ) return;
    g->superzaps--;
    show_message( g, "SUPERZAPPER!" );
    int i;
    for( i = 0; i < MAX_ENEMIES; i++ )
    {
        if( g->ENEMIES[ i ].alive )
        {
            project( g, g->ENEMIES[ i ].lane, g->ENEMIES[ i ].z );
            explode( g, g->px, g->py, 18, 3 );
            g->ENEMIES[ i ].alive = 0;
            g->score += 150;
        }
    }
    // the zap can wipe out the last enemies — check for level clear
    if( g->spawn_timer > 999 && count_enemies( g ) == 0 ) level_clear( g );
}

void update_player( G* g )
{
    if( gamepad_left() > 0 )  g->player_lane -= 0.09;
    if( gamepad_right() > 0 ) g->player_lane += 0.09;
    if( g->player_lane < 0 ) g->player_lane += LANES;
    if( g->player_lane >= LANES ) g->player_lane -= LANES;

    // camera sway: ease the vanishing point toward a fraction of the
    // claw's offset from the tube axis — the tube appears to lean and
    // follow you around the web (Tempest 2000 flavour)
    project( g, g->player_lane, 0 );
    float camtx = ( g->px - CX ) * CAM_SWAY;
    float camty = ( g->py - CY ) * CAM_SWAY;
    g->cam_x += ( camtx - g->cam_x ) * CAM_EASE;
    g->cam_y += ( camty - g->cam_y ) * CAM_EASE;

    if( gamepad_button_a() > 0 ) fire( g );
    if( gamepad_button_b() == 1 ) superzap( g );   // == 1: just-pressed edge
    if( g->fire_cooldown > 0 ) g->fire_cooldown--;

    // JUMP (button Y): the claw leaps off the web toward the camera —
    // airborne frames are invulnerable at the rim, so you can hop
    // over a camper instead of only sliding away from it
    if( g->jump_cooldown > 0 ) g->jump_cooldown--;
    if( g->jump_timer > 0 ) g->jump_timer--;
    else if( gamepad_button_y() == 1 && g->jump_cooldown == 0 )
    {
        g->jump_timer = 34;
        g->jump_cooldown = 60;   // lands at 34, 26 frames of recovery
        burst( g, g->px, g->py, 8, 1.5 );   // takeoff puff
    }
}

void kill_player( G* g )
{
    g->state = 1;
    g->state_timer = 110;
    g->lives--;
    project( g, g->player_lane, 0 );
    explode( g, g->px, g->py, 70, 5 );
    shockwave( g, g->px, g->py );   // double ring: both slots
    show_message( g, "OW!" );
}

void level_clear( G* g );

// ---------------------------------------------------------------------------
//  Power-ups: shoot the pods to collect them.
//    type 0 = SUPERZAP refill, type 1 = AI buddy, type 2 = extra life
// ---------------------------------------------------------------------------
void collect_powerup( G* g, int i )
{
    if( g->POWERUPS[ i ].type == 0 )
    {
        if( g->superzaps < 4 )
        {
            g->superzaps++;
            show_message( g, "SUPERZAP RECHARGED!" );
        }
        else
        {
            g->score += 250;
            show_message( g, "BONUS 250!" );
        }
    }
    else if( g->POWERUPS[ i ].type == 1 )
    {
        g->buddy_timer = 12 * 60;
        show_message( g, "AI BUDDY ONLINE!" );
    }
    else
    {
        if( g->lives < 4 )
        {
            g->lives++;
            show_message( g, "EXTRA LIFE!" );
        }
        else
        {
            g->score += 250;
            show_message( g, "BONUS 250!" );
        }
    }
    project( g, g->POWERUPS[ i ].lane, g->POWERUPS[ i ].z );
    explode( g, g->px, g->py, 20, 2.5 );
    g->POWERUPS[ i ].alive = 0;
}

void spawn_powerup( G* g )
{
    int i;
    for( i = 0; i < 4; i++ )
    {
        if( g->POWERUPS[ i ].alive ) continue;
        g->POWERUPS[ i ].alive = 1;
        g->POWERUPS[ i ].lane = rng( g ) % LANES;
        g->POWERUPS[ i ].z = 1.0;
        int r = rng( g ) % 20;
        if( r < 9 )       g->POWERUPS[ i ].type = 0;
        else if( r < 18 )  g->POWERUPS[ i ].type = 1;
        else               g->POWERUPS[ i ].type = 2;
        // don't offer superzap refills when the player is already at
        // the 4-charge cap — a wasted pod. Convert to a buddy instead.
        if( g->POWERUPS[ i ].type == 0 && g->superzaps >= 4 )
            g->POWERUPS[ i ].type = 1;
        return;
    }
}

void update_powerups( G* g )
{
    int i;
    for( i = 0; i < 4; i++ )
    {
        if( !g->POWERUPS[ i ].alive ) continue;
        g->POWERUPS[ i ].z -= 0.0016;          // drift toward the rim
        if( g->POWERUPS[ i ].z < 0.06 ) g->POWERUPS[ i ].alive = 0;
    }
    g->powerup_timer--;
    if( g->powerup_timer <= 0 )
    {
        g->powerup_timer = 700 + rng( g ) % 700;
        int n = 0;
        for( i = 0; i < 4; i++ ) if( g->POWERUPS[ i ].alive ) n++;
        if( n < 2 ) spawn_powerup( g );
    }
}

// AI buddy: hovers by the claw, auto-fires at the enemy closest to the
// player's lane every 30 frames while its timer lasts
void update_buddy( G* g )
{
    if( g->buddy_timer <= 0 ) return;
    g->buddy_timer--;
    if( g->buddy_cooldown > 0 ) { g->buddy_cooldown--; return; }

    int best = -1;
    float bestdiff = 999.0;
    int i;
    for( i = 0; i < MAX_ENEMIES; i++ )
    {
        if( !g->ENEMIES[ i ].alive ) continue;
        float diff = g->ENEMIES[ i ].lane - g->player_lane;
        if( diff < 0 ) diff = -diff;
        if( diff > LANES / 2 ) diff = LANES - diff;
        if( diff < bestdiff ) { bestdiff = diff; best = i; }
    }
    if( best >= 0 )
    {
        int j;
        for( j = 0; j < MAX_BULLETS; j++ )
        {
            if( !g->BULLETS[ j ].alive )
            {
                g->BULLETS[ j ].alive = 1;
                g->BULLETS[ j ].lane = g->ENEMIES[ best ].lane;
                g->BULLETS[ j ].z = 0.10;
                g->buddy_cooldown = 30;
                break;
            }
        }
    }
}

void update_enemies( G* g )
{
    int i;
    for( i = 0; i < MAX_ENEMIES; i++ )
    {
        if( !g->ENEMIES[ i ].alive ) continue;

        float speed = 0.0035 + g->level * 0.0004;

        // spiker: patrols mid-tunnel laying spikes
        if( g->ENEMIES[ i ].type == 2 )
        {
            if( g->ENEMIES[ i ].cooldown > 0 ) g->ENEMIES[ i ].cooldown--;
            else
            {
                g->ENEMIES[ i ].lane += 1;
                if( g->ENEMIES[ i ].lane >= LANES ) g->ENEMIES[ i ].lane -= LANES;
                g->ENEMIES[ i ].cooldown = 26;
            }
            int li = ( (int)g->ENEMIES[ i ].lane ) % LANES;
            if( g->SPIKE[ li ] < 0.55 ) g->SPIKE[ li ] += 0.006;
            continue;
        }

        g->ENEMIES[ i ].z -= speed;

        // flipper: switches lanes when near the rim, hunting the player
        if( g->ENEMIES[ i ].type == 0 && g->ENEMIES[ i ].z < 0.45 )
        {
            if( g->ENEMIES[ i ].cooldown > 0 ) g->ENEMIES[ i ].cooldown--;
            else
            {
                float diff = g->player_lane - g->ENEMIES[ i ].lane;
                if( diff > LANES / 2 ) diff -= LANES;
                if( diff < -LANES / 2 ) diff += LANES;
                if( diff > 0.5 ) g->ENEMIES[ i ].lane += 1;
                else if( diff < -0.5 ) g->ENEMIES[ i ].lane -= 1;
                if( g->ENEMIES[ i ].lane < 0 ) g->ENEMIES[ i ].lane += LANES;
                if( g->ENEMIES[ i ].lane >= LANES ) g->ENEMIES[ i ].lane -= LANES;
                g->ENEMIES[ i ].cooldown = 14;
            }
        }

        // rim behaviour: lethal if it shares your lane — unless the
        // claw is mid-jump (hop over the camper)
        if( g->ENEMIES[ i ].z < 0 )
        {
            g->ENEMIES[ i ].z = 0;
            float diff = g->player_lane - g->ENEMIES[ i ].lane;
            if( diff < 0 ) diff = -diff;
            if( diff > LANES / 2 ) diff = LANES - diff;
            if( diff < 0.7 && g->jump_timer <= 0 ) { kill_player( g ); }
        }
    }
}

void update_bullets( G* g )
{
    int i; int j;
    for( i = 0; i < MAX_BULLETS; i++ )
    {
        if( !g->BULLETS[ i ].alive ) continue;
        g->BULLETS[ i ].z += 0.035;
        if( g->BULLETS[ i ].z > 1.0 ) { g->BULLETS[ i ].alive = 0; continue; }

        // spikes eat bullets: trim the spike, kill the shot
        int li = ( (int)g->BULLETS[ i ].lane ) % LANES;
        if( g->SPIKE[ li ] > 0 && g->BULLETS[ i ].z >= 1.0 - g->SPIKE[ li ] )
        {
            g->SPIKE[ li ] -= 0.10;
            if( g->SPIKE[ li ] < 0 ) g->SPIKE[ li ] = 0;
            project( g, g->BULLETS[ i ].lane, g->BULLETS[ i ].z );
            burst( g, g->px, g->py, 6, 1.5 );
            g->BULLETS[ i ].alive = 0;
            g->score += 5;
            continue;
        }

        // collect power-up pods by shooting them
        int collected = 0;
        int p;
        for( p = 0; p < 4; p++ )
        {
            if( !g->POWERUPS[ p ].alive ) continue;
            float dzp = g->BULLETS[ i ].z - g->POWERUPS[ p ].z;
            if( dzp < 0 ) dzp = -dzp;
            float dlp = g->BULLETS[ i ].lane - g->POWERUPS[ p ].lane;
            if( dlp < 0 ) dlp = -dlp;
            if( dlp > LANES / 2 ) dlp = LANES - dlp;
            if( dzp < 0.04 && dlp < 0.6 )
            {
                g->BULLETS[ i ].alive = 0;
                collect_powerup( g, p );
                collected = 1;
                break;
            }
        }
        if( collected ) continue;

        // collision vs enemies
        for( j = 0; j < MAX_ENEMIES; j++ )
        {
            if( !g->ENEMIES[ j ].alive ) continue;
            float dz = g->BULLETS[ i ].z - g->ENEMIES[ j ].z;
            if( dz < 0 ) dz = -dz;
            float dl = g->BULLETS[ i ].lane - g->ENEMIES[ j ].lane;
            if( dl < 0 ) dl = -dl;
            if( dl > LANES / 2 ) dl = LANES - dl;
            if( dz < 0.03 && dl < 0.6 )
            {
                g->BULLETS[ i ].alive = 0;
                project( g, g->ENEMIES[ j ].lane, g->ENEMIES[ j ].z );
                explode( g, g->px, g->py, 26, 3.0 );

                if( g->ENEMIES[ j ].type == 1 )
                {
                    // tanker splits into two flippers
                    g->ENEMIES[ j ].type = 0;
                    g->score += 100;
                    spawn_near( g, g->ENEMIES[ j ].lane + 1, g->ENEMIES[ j ].z );
                }
                else
                {
                    g->ENEMIES[ j ].alive = 0;
                    g->score += 100;
                    if( g->spawn_timer > 999 && count_enemies( g ) == 0 )
                        level_clear( g );
                }
                break;
            }
        }
    }
}

void level_clear( G* g )
{
    g->state = 2;
    g->state_timer = 160;
    g->warp = 0;
    show_message( g, "EXCELLENT!" );
    g->score += 1000 + g->level * 250;
}

void update_shocks( G* g )
{
    int i;
    for( i = 0; i < 3; i++ )
    {
        if( !g->SHOCKS[ i ].alive ) continue;
        g->SHOCKS[ i ].life--;
        if( g->SHOCKS[ i ].life <= 0 ) g->SHOCKS[ i ].alive = 0;
    }
}

void update_particles( G* g )
{
    int i;
    for( i = 0; i < MAX_PARTICLES; i++ )
    {
        if( !g->PARTICLES[ i ].alive ) continue;
        g->PARTICLES[ i ].x += g->PARTICLES[ i ].vx;
        g->PARTICLES[ i ].y += g->PARTICLES[ i ].vy;
        g->PARTICLES[ i ].vx *= 0.96;
        g->PARTICLES[ i ].vy *= 0.96;
        g->PARTICLES[ i ].life--;
        if( g->PARTICLES[ i ].life <= 0 ) g->PARTICLES[ i ].alive = 0;
    }
}

// ---------------------------------------------------------------------------
//  Background starfield — mild, cheap, always on
// ---------------------------------------------------------------------------
void init_starfield( G* g )
{
    int i;
    for( i = 0; i < MAX_STARS; i++ )
    {
        g->STARFIELD[ i ].x = frand( g ) * 640;
        g->STARFIELD[ i ].y = frand( g ) * 336;
        g->STARFIELD[ i ].spd = 0.25 + frand( g ) * 0.5;
    }
}

void update_stars( G* g )
{
    int i;
    // warp-out: the ship surges forward, so the stars streak
    float boost = 1.0 + g->warp * 6.0;
    for( i = 0; i < MAX_STARS; i++ )
    {
        float dx = g->STARFIELD[ i ].x - CX;
        float dy = g->STARFIELD[ i ].y - CY;
        float d = sqrt( dx * dx + dy * dy );
        if( d < 1 ) d = 1;
        g->STARFIELD[ i ].x += dx / d * g->STARFIELD[ i ].spd * boost;
        g->STARFIELD[ i ].y += dy / d * g->STARFIELD[ i ].spd * boost;
        if( g->STARFIELD[ i ].x < -4 || g->STARFIELD[ i ].x > 644 ||
            g->STARFIELD[ i ].y < -4 || g->STARFIELD[ i ].y > 340 )
        {
            // respawn near the vanishing point so the stream is endless
            float a = frand( g ) * 6.28318;
            float r = 4 + frand( g ) * 30;
            g->STARFIELD[ i ].x = CX + cos32( g, a ) * r;
            g->STARFIELD[ i ].y = CY + sin32( g, a ) * r;
            g->STARFIELD[ i ].spd = 0.25 + frand( g ) * 0.5;
        }
    }
}

void render_starfield( G* g )
{
    int i;
    set_blending_mode( BLEND_SOLID );
    for( i = 0; i < MAX_STARS; i++ )
    {
        if( g->STARFIELD[ i ].spd > 0.45 )
            set_multiply_color( make_color( 140, 150, 180 ) );
        else
            set_multiply_color( make_color( 70, 80, 110 ) );
        draw_glyph( g, '.', g->STARFIELD[ i ].x, g->STARFIELD[ i ].y, 3, 3 );
    }
}

// ---------------------------------------------------------------------------
//  Rendering
// ---------------------------------------------------------------------------
void render_web( G* g )
{
    int i; int r;

    // WEB BARS DRAW UNDER ALPHA, not mode 0: the one draw_segment
    // path proven to render on the real emulator is the spikes'
    // (set_glow -> BlendAlpha). Mode-0 draws of region 20 with a
    // custom hotspot have never appeared. Opaque colors + alpha
    // blending = same visual result as solid for our purposes.
    set_blending_mode( v32::BlendAlpha );

    // lane edges (SPOKES): production build — dim teal-blue, thickness 2.
    // (The bright-green + white-marker variant was a diagnostic build
    // for the sin_taylor misalignment; that fix is confirmed good.)
    set_multiply_color( make_color( 25, 70, 110 ) );
    for( i = 0; i < LANES; i++ )
    {
        project( g, i, 0 );  float x0 = g->px; float y0 = g->py;
        project( g, i, 1 );  float x1 = g->px; float y1 = g->py;
        draw_segment( g, x0, y0, x1, y1, 2.0 );
    }

    // far end cap: full outline, every lane joined (16 bars) — 2-lane
    // chords cut corners and left gaps that read as detached segments
    set_multiply_color( make_color( 40, 90, 130 ) );
    for( i = 0; i < LANES; i++ )
    {
        project( g, i, 1 );     float x0 = g->px; float y0 = g->py;
        project( g, i + 1, 1 ); draw_segment( g, x0, y0, g->px, g->py, 2.0 );
    }

    // far-cap vertex caps: the cap polygon is tiny, so a 1-2px angular
    // miss at a spoke joint reads as a clear disconnect (most visible
    // on the right side). Small blocks at each cap vertex cover the
    // joints, matching the rim's point markers. 16 cheap zoomed draws.
    for( i = 0; i < LANES; i++ )
    {
        project( g, i, 1 );
        select_region( 20 );
        set_drawing_scale( 0.5, 0.35 );     // ~5 x 7 px block
        draw_region_zoomed_at( (int)( g->px - 3 ), (int)( g->py - 3 ) );
        g->last_region = -1;
    }

    // depth rings: cheap zoomed DOTS (the rotozoomed command is the
    // emulator's slow path — rings move every frame, so they get the
    // budget treatment). One dot per lane per ring.
    float scroll = 0.002 + g->warp * g->warp * 0.12;
    for( r = 0; r < RINGS; r++ )
    {
        float z = ( ( g->frame * scroll ) + r / ( RINGS * 1.0 ) );
        while( z > 1 ) z -= 1;
        set_multiply_color( make_color( 20, 60 + r * 6, 90 ) );
        for( i = 0; i < LANES; i++ )
        {
            project( g, i + 0.5, z );
            float s = 3.0 + z * 3.0;
            draw_glyph( g, '.', g->px, g->py, s, s );
        }
    }

    // the rim: bright neon outline, every lane joined (16 bars), pulsing
    int pulse = 200 + (int)( sin32( g, g->frame * 0.1 ) * 55 );
    set_multiply_color( make_color( 60, 180, pulse ) );
    for( i = 0; i < LANES; i++ )
    {
        project( g, i, 0 );     float x0 = g->px; float y0 = g->py;
        project( g, i + 1, 0 ); draw_segment( g, x0, y0, g->px, g->py, 3.5 );
    }

    // vertex caps: a small block at each rim vertex, exactly like the
    // point markers on Tempest's web. Hides the seam gap at the top
    // wrap (lane 15 -> 16) and caps any chord ends that stick out
    // past a joint. 16 cheap zoomed draws.
    for( i = 0; i < LANES; i++ )
    {
        project( g, i, 0 );
        select_region( 20 );
        set_drawing_scale( 0.55, 0.3 );     // ~5.5 x 6 px block
        draw_region_zoomed_at( (int)( g->px - 3 ), (int)( g->py - 3 ) );
        g->last_region = -1;
    }
}

// spikes: solid red bars from the far end down toward the rim
void render_spikes( G* g )
{
    int i;
    set_glow( 2 );
    set_multiply_color( make_color( 255, 60, 30 ) );
    for( i = 0; i < LANES; i++ )
    {
        if( g->SPIKE[ i ] <= 0 ) continue;
        project( g, i, 1.0 - g->SPIKE[ i ] );  float x0 = g->px; float y0 = g->py;
        project( g, i, 1.0 );
        draw_segment( g, x0, y0, g->px, g->py, 3.0 );
        // hot tip
        project( g, i, 1.0 - g->SPIKE[ i ] );
        draw_glyph( g, '+', g->px, g->py, 8, 8 );
    }
    set_blending_mode( BLEND_SOLID );
}

void render_player( G* g )
{
    if( g->state == 1 ) return;   // dying: particles only

    // transition rework: the claw NO LONGER dives into the tube on
    // warp-out — it stays parked on the rim while the web streams
    // past it (see the fly factor in project())
    project( g, g->player_lane, 0 );
    float x = g->px; float y = g->py; float s = g->pscale;

    // jump arc: airborne claw pops toward the camera — bigger and
    // pushed radially outward from the tube axis (T2K-style leap)
    if( g->jump_timer > 0 )
    {
        float jarc = sin32( g, 3.14159 * ( 34 - g->jump_timer ) / 34.0 );
        x += ( x - CX ) * jarc * 0.18;
        y += ( y - CY ) * jarc * 0.18;
        s *= ( 1.0 + jarc * 1.2 );
    }

    set_blending_mode( BLEND_SOLID );
    set_multiply_color( make_color( 255, 220, 60 ) );
    draw_glyph( g, 'X', x, y, 20 * s, 24 * s );

    set_glow( 3 );
    set_multiply_color( make_color( 255, 120, 40 ) );
    draw_glyph( g, 'O', x, y - 8 * s, 12 * s, 12 * s );
    draw_glyph( g, '<', x - 16 * s, y - 2 * s, 12 * s, 14 * s );
    draw_glyph( g, '>', x + 16 * s, y - 2 * s, 12 * s, 14 * s );
    set_blending_mode( BLEND_SOLID );
}

void render_enemies( G* g )
{
    int i;
    for( i = 0; i < MAX_ENEMIES; i++ )
    {
        if( !g->ENEMIES[ i ].alive ) continue;
        project( g, g->ENEMIES[ i ].lane, g->ENEMIES[ i ].z );
        float x = g->px; float y = g->py; float s = g->pscale;

        float wob = 0;
        if( g->ENEMIES[ i ].z <= 0 )
            wob = sin32( g, g->ENEMIES[ i ].wig + g->frame * 0.2 ) * 4;

        set_blending_mode( BLEND_SOLID );
        if( g->ENEMIES[ i ].type == 0 )
        {
            set_multiply_color( hue( 160 + ( ( i * 40 + g->frame ) >> 2 ) ) );
            draw_glyph( g, 'W', x, y + wob, 26 * s, 20 * s );
            set_glow( 4 );
            draw_glyph( g, '*', x, y + wob, 14 * s, 14 * s );
        }
        else if( g->ENEMIES[ i ].type == 1 )
        {
            set_multiply_color( make_color( 255, 80, 160 ) );
            draw_glyph( g, 'H', x, y, 26 * s, 24 * s );
            set_glow( 4 );
            draw_glyph( g, '#', x, y, 14 * s, 14 * s );
        }
        else
        {
            set_multiply_color( make_color( 255, 150, 40 ) );
            draw_glyph( g, 'M', x, y, 22 * s, 18 * s );
            set_glow( 4 );
            draw_glyph( g, 'v', x, y + 10 * s, 12 * s, 10 * s );
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
        set_multiply_color( make_color( 255, 200 - (int)( g->BULLETS[ i ].z * 180 ),
                                       120 - (int)( g->BULLETS[ i ].z * 100 ) ) );
        draw_glyph( g, '*', g->px, g->py, 10 * s + 3, 10 * s + 3 );
    }
    set_blending_mode( BLEND_SOLID );
}

void render_particles( G* g )
{
    int i;
    set_glow( 1 );
    for( i = 0; i < MAX_PARTICLES; i++ )
    {
        if( !g->PARTICLES[ i ].alive ) continue;
        float t = 1.0 - ( g->PARTICLES[ i ].life / 50.0 );
        if( t < 0 ) t = 0;
        int f = (int)( t * 220 );
        // color varies by glyph: 'o'/'O' particles burn white-hot,
        // the rest go orange-to-red as they age. Sizes twinkle so
        // bursts sparkle instead of just fading.
        int gl = g->PARTICLES[ i ].glyph;
        if( gl >= 4 )
            set_multiply_color( make_color( 255, 250 - f / 3, 200 - f ) );
        else
            set_multiply_color( make_color( 255, 220 - f, 120 - f / 2 ) );
        float s = 4 + gl * 1.5;
        if( ( i + g->frame ) % 3 == 0 ) s *= 1.4;   // twinkle
        draw_glyph( g, g->GLYPHS[ gl ],
                    g->PARTICLES[ i ].x, g->PARTICLES[ i ].y, s, s );
    }
    set_blending_mode( BLEND_SOLID );
}

// shockwave rings: a growing 12-sided polygon of vector bars that
// expands from 6px to ~46px radius while fading from white to deep red
void render_shocks( G* g )
{
    int i; int k;
    set_blending_mode( v32::BlendAlpha );
    for( i = 0; i < 3; i++ )
    {
        if( !g->SHOCKS[ i ].alive ) continue;
        float t = g->SHOCKS[ i ].life / 22.0;      // 1 -> 0
        float r = 6 + ( 1.0 - t ) * 40;
        int fade = (int)( t * 160 );
        set_multiply_color( make_color( 255, 100 + fade, 60 + fade / 2 ) );
        float px0 = 0; float py0 = 0;
        for( k = 0; k <= 12; k++ )
        {
            float a = k * 0.523598776;            // 2*PI/12
            float x = g->SHOCKS[ i ].x + cos32( g, a ) * r;
            float y = g->SHOCKS[ i ].y + sin32( g, a ) * r;
            if( k > 0 ) draw_segment( g, px0, py0, x, y, 1.0 + t * 2.0 );
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
    for( i = 0; i < 4; i++ )
    {
        if( !g->POWERUPS[ i ].alive ) continue;
        project( g, g->POWERUPS[ i ].lane, g->POWERUPS[ i ].z );
        float pulse = 0.8 + 0.25 * sin32( g, g->frame * 0.25 );
        float s = ( 12 * g->pscale + 3 ) * pulse;
        int col;
        if( g->POWERUPS[ i ].type == 0 )      col = make_color( 255, 0, 255 );
        else if( g->POWERUPS[ i ].type == 1 ) col = make_color( 60, 255, 60 );
        else                                  col = make_color( 255, 220, 60 );
        set_blending_mode( BLEND_SOLID );
        set_multiply_color( col );
        draw_glyph( g, 'O', g->px, g->py, s, s * 0.8 );
        set_glow( 4 );
        draw_glyph( g, '*', g->px, g->py, s * 0.6, s * 0.6 );
        set_blending_mode( BLEND_SOLID );
    }
}

// AI buddy drone: hovers beside the claw, bobbing; blinks out during
// its final two seconds
void render_buddy( G* g )
{
    if( g->buddy_timer <= 0 ) return;
    if( g->buddy_timer < 120 && ( g->frame % 8 ) < 3 ) return;
    float off = 1.9;
    project( g, g->player_lane + off, 0.10 );
    float x = g->px;
    float y = g->py + sin32( g, g->frame * 0.15 ) * 4;
    set_blending_mode( BLEND_SOLID );
    set_multiply_color( make_color( 120, 255, 160 ) );
    draw_glyph( g, 'W', x, y, 10, 10 );
    set_glow( 4 );
    draw_glyph( g, '*', x, y, 6, 6 );
    set_blending_mode( BLEND_SOLID );
}

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

    // lives as claw icons
    int l;
    set_blending_mode( BLEND_SOLID );
    set_multiply_color( make_color( 255, 220, 60 ) );
    for( l = 0; l < g->lives; l++ )
        draw_glyph( g, 'X', 560 + l * 22, 344, 14, 14 );

    // level
    char lvl[ 7 ];
    lvl[ 0 ] = 'L'; lvl[ 1 ] = 'V'; lvl[ 2 ] = 'L'; lvl[ 3 ] = ' ';
    lvl[ 4 ] = '0' + ( g->level / 10 ) % 10;
    lvl[ 5 ] = '0' + g->level % 10;
    lvl[ 6 ] = 0;
    draw_text( g, lvl, 20, 344, 12, make_color( 120, 200, 255 ) );

    // superzapper charges
    set_multiply_color( make_color( 255, 255, 255 ) );
    for( l = 0; l < g->superzaps; l++ )
        draw_glyph( g, 'Z', 288 + l * 18, 344, 12, 12 );

    // AI buddy countdown (seconds remaining) while it is online
    if( g->buddy_timer > 0 )
    {
        char bud[ 6 ];
        int secs = g->buddy_timer / 60;
        bud[ 0 ] = 'A'; bud[ 1 ] = 'I'; bud[ 2 ] = ' ';
        bud[ 3 ] = '0' + ( secs / 10 ) % 10;
        bud[ 4 ] = '0' + secs % 10;
        bud[ 5 ] = 0;
        draw_text( g, bud, 372, 344, 12, make_color( 120, 255, 160 ) );
    }
}

void render_message( G* g )
{
    if( g->message_timer <= 0 ) return;
    draw_party_text( g, g->message, 320, 70, 22, 90 );
    g->message_timer--;
}

// ---------------------------------------------------------------------------
//  Level flow
// ---------------------------------------------------------------------------
void start_level( G* g )
{
    int i;
    for( i = 0; i < MAX_ENEMIES; i++ ) g->ENEMIES[ i ].alive = 0;
    for( i = 0; i < MAX_BULLETS; i++ ) g->BULLETS[ i ].alive = 0;
    g->superzaps = 2;
    for( i = 0; i < LANES; i++ ) g->SPIKE[ i ] = 0;
    g->launched = 0;
    make_shape( g );
    g->spawn_interval = 70 - g->level * 2;
    if( g->spawn_interval < 18 ) g->spawn_interval = 18;
    g->spawn_timer = 40;
    g->state = 0;
    show_message( g, "LEVEL READY" );
}

void update_spawning( G* g )
{
    g->spawn_timer--;
    if( g->spawn_timer <= 0 )
    {
        g->spawn_timer = g->spawn_interval;
        int roll = rng( g ) % 7;
        int tanker = ( roll == 0 );
        int spiker = ( roll == 1 );
        spawn_enemy( g, spiker ? 2 : ( tanker ? 1 : 0 ) );
        g->launched++;
        if( g->launched >= 18 + g->level )
        {
            g->spawn_timer = 999999;   // stop spawning; level ends when clear
            g->launched = 0;
        }
    }
}

// ---------------------------------------------------------------------------
//  Main
// ---------------------------------------------------------------------------
void init_state( G* g )
{
    g->GLYPHS[ 0 ] = '.';
    g->GLYPHS[ 1 ] = '+';
    g->GLYPHS[ 2 ] = '*';
    g->GLYPHS[ 3 ] = 'o';
    g->GLYPHS[ 4 ] = 'O';
    g->GLYPHS[ 5 ] = '#';
    g->player_lane = 0;
    g->cam_x = 0;
    g->cam_y = 0;
    g->lives = 3;
    g->score = 0;
    g->level = 1;
    g->warp = 0;
    g->frame = 0;
    g->rng_state = 12345;
    g->last_region = -1;
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;

    // effects + power-up state: heap memory from `new` is NOT zeroed,
    // so every slot must be explicitly cleared here
    g->buddy_timer = 0;
    g->buddy_cooldown = 0;
    g->powerup_timer = 500;
    g->jump_timer = 0;
    g->jump_cooldown = 0;
    int i;
    for( i = 0; i < 3; i++ ) g->SHOCKS[ i ].alive = 0;
    for( i = 0; i < 4; i++ ) g->POWERUPS[ i ].alive = 0;
    build_tables( g );
    init_starfield( g );
}

void main()
{
    select_texture( -1 );           // BIOS font
    select_gamepad( 0 );

    // ALL mutable state lives on the heap: file-scope variables in
    // generated Vircon32 C are read-only (no `global` keyword emitted)
    G* g = new G;
    init_state( g );
    // start_level (not just init_state) is required at boot: it is
    // what actually zeroes enemies/bullets/spikes, grants the 2
    // superzaps and sets the spawn timers + state=0. Heap memory is
    // not guaranteed zeroed, so relying on that was luck.
    start_level( g );
    make_shape( g );
    show_message( g, "TEMPEST 32K" );

    while( 1 )
    {
        // -- update ---------------------------------------------------------
        if( g->state == 0 )
        {
            update_player( g );
            update_spawning( g );
            update_enemies( g );
            update_bullets( g );
            update_powerups( g );
            update_buddy( g );
        }
        else if( g->state == 1 )      // dying
        {
            g->state_timer--;
            if( g->state_timer <= 0 )
            {
                if( g->lives <= 0 ) { g->state = 3; show_message( g, "GAME OVER" ); g->state_timer = 300; }
                else start_level( g );
            }
        }
        else if( g->state == 2 )      // warp-out: web flies past the claw
        {
            // purely cosmetic now — the claw parks on the rim and the
            // geometry streams outward (fly factor in project()), so
            // the old mid-warp spike collision no longer applies
            g->warp += 0.012;
            g->state_timer--;
            if( g->state_timer <= 0 ) { g->level++; g->warp = 0; start_level( g ); }
        }
        else if( g->state == 3 )      // game over
        {
            g->state_timer--;
            if( g->state_timer <= 0 )
            {
                init_state( g );
                start_level( g );
                g->state = 0;
            }
        }
        update_particles( g );
        update_shocks( g );
        update_stars( g );
        g->frame++;

        // -- render ---------------------------------------------------------
        // insurance: force known GPU state before the clear, so no
        // stale blending mode or multiply color can interfere with it
        set_blending_mode( BLEND_SOLID );
        set_multiply_color( make_color( 255, 255, 255 ) );
        clear_screen( make_color( 2, 2, 8 ) );
        render_starfield( g );
        render_web( g );
        render_spikes( g );
        render_bullets( g );
        render_enemies( g );
        render_powerups( g );
        render_buddy( g );
        render_player( g );
        render_particles( g );
        render_shocks( g );
        render_hud( g );
        render_message( g );

        end_frame();
    }
}
