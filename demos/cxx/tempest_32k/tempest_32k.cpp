// *****************************************************************************
//  TEMPEST 32K  —  a Tempest 2000-style tunnel shooter for Vircon32,
//  written in the v32c++ subset, rendered entirely with ASCII glyphs from
//  the BIOS font texture (-1). No custom textures, no 3D hardware.
//  Revision: walker tumble + pause text size 14 + busy-frame draw culls.
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
//    * No STL, no templates. Ternary: DO NOT USE — the rewrite leaked a
//      raw '?' into the generated C once (Vircon32 C lexer: "character
//      '?' is not a valid identifier start"). if/else always; see the
//      limitations doc. Array initializer lists are supported by
//      current v32c++ builds, but this file still avoids them — arrays
//      are filled at runtime; see VIRCON32_QUIRKS.md / README notes.
//    * `main` is void (transpiler emits `void main(void)`).
// *****************************************************************************

#include <v32/video.hpp>
#include <v32/input.hpp>
#include <v32/time.hpp>
#include <v32/math.hpp>     // sqrt() for segment lengths (hardware pow)
#include "audio.h"         // SPU: stop/assign/play channel, channel states
#include "memcard.h"       // memory card: card_is_connected/read/write data

// SPU channel-state register values (audio.h reads them raw): the
// music succession logic compares against these
#define CH_STOPPED 0x40
#define CH_PAUSED  0x41
#define CH_PLAYING 0x42

// 0 = solid (no blending); the v32 enum only names alpha/add/subtract
#define BLEND_SOLID 0

// ---------------------------------------------------------------------------
//  Cart metadata (parsed by the v32c++ lexer; delete if your build rejects)
// ---------------------------------------------------------------------------
#title "TEMPEST 32K"
#version 0.3

// CART SOUND RESOURCES — v32c++ lexer hints (#sound NAME "file"). The
// transpiler emits `#define NAME id` in declaration order, so NAME is a
// compile-time constant usable anywhere below. IDs M_TRACK1..M_TRACK4
// are CONSECUTIVE (music succession relies on M_TRACK1 + n arithmetic).
// Generate the .wav files with the companion gen_sounds.c program.
#sound PEWPEW  "sounds/shootsound.wav"
#sound BOOM    "sounds/boom.wav"
#sound DEATH   "sounds/death.wav"
#sound ZAPSND  "sounds/zap.wav"
#sound JUMP    "sounds/jump.wav"
#sound PICKUP  "sounds/pickup.wav"
#sound CLEAR   "sounds/clear.wav"
#sound BLIP    "sounds/blip.wav"
#sound M_TITLE  "sounds/m_title.wav"
#sound M_TRACK1 "sounds/m_track1.wav"
#sound M_TRACK2 "sounds/m_track2.wav"
#sound M_TRACK3 "sounds/m_track3.wav"
#sound M_TRACK4 "sounds/m_track4.wav"

// ---------------------------------------------------------------------------
//  Tunables — #define, NOT const variables: reading a const on the RHS of
//  an assignment is a hard error in Vircon32 C ("discards const qualifier")
// ---------------------------------------------------------------------------
#define LANES 16          // web lanes (array sizes below spell this as 16!)
#define RINGS 7
#define MAX_BULLETS 32
#define MAX_ENEMIES 24
#define MAX_PARTICLES 220
#define MAX_STARS 80

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
    int type;          // 0 = flipper, 1 = tanker, 2 = spiker, 3 = rim walker ('X')
    float lane;
    float z;
    int cooldown;
    int wig;
    int dir;           // patrol direction (+1/-1): spikers mid-tunnel,
                       // rim walkers along the edge; both reverse at gaps
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

// background starfield: screen-space stars streaming outward from the
// vanishing point (matches the fly-into-the-tunnel camera), streaking
// during warp-out. Bright and plainly visible — the original dim/slow
// version was practically invisible against the dark background.
struct Star
{
    float x; float y;
    float spd;               // radial px/frame
};

struct PowerUp
{
    int alive;
    int type;        // 0 = superzap, 1 = AI buddy, 2 = extra life,
                     // 3 = super laser, 4 = rapid blaster
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
    float SHAPE_Y[ 16 ];   // vertical radius factor: 1.0 on the CLASSIC
                           // pass (levels 1-16) so the shape modulates
                           // only x; equal to SHAPE on the TRUE pass
                           // (17-32) so webs trace their real polygon
    int   CONN[ 16 ];          // CONN[i]: 1 = rim edge between lanes i,i+1 exists
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
    Star     STARFIELD[ 80 ];
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
    int   state;               // 0 play, 1 dying, 2 warp-out, 3 game over, 4 title, 5 pause, 6 initials, 7 scores, 8 level select
    int   warp_phase;          // transition: 0 = old web flying out, 1 = new web flying in
    int   warp_bounce;         // transition: spike hit on EASY — skip the level advance
    int   menu_row;            // title menu: selected row (0 play 1 scores 2 levels 3 difficulty)
    int   select_level;        // level select screen: chosen level (1..32)
    int   HISCORE[ 5 ];        // high score table, highest first (memcard)
    int   HIINIT[ 5 ][ 3 ];    // 3-letter initials per high score entry
    int   hs_rank;             // rank of the score currently being entered
    int   entry_pos;           // initials entry: cursor 0..2
    int   entry_letters[ 3 ];  // initials entry: current letters (char codes)
    int   difficulty;          // 0 easy, 1 medium, 2 hard (set on title screen)
    int   menu_cooldown;       // title screen: frames between L/R difficulty nudges
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
    float buddy_lane;       // AI buddy: its OWN lane (eases to targets)
    int   laser_timer;      // super laser power-up: frames remaining
    int   rapid_timer;      // rapid blaster power-up: frames remaining
                             // (easy uses 999999 = effectively unlimited)
    int   powerup_timer;    // frames until the next power-up spawns
    int   music_index;      // gameplay track currently queued (0..3)
    int   sfx_channel;      // round-robin SFX channel allocator (2..13)
    float music_volume;     // channel 0 volume 0..2 (gameplay + title tracks)
    float sfx_volume;       // channels 2..13 volume 0..2 (effects)
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

// draw BIOS region c (a 10x20 cell) CENTRED at (x,y) and rotated a
// radians clockwise (y-down screen) — via the ROTOZOOMED command, the
// same proven path draw_segment uses. The draw pivots on the region
// HOTSPOT (top-left), so the local centre (w/2, h/2) is rotated and
// subtracted to land the glyph centred on (x,y).
void draw_rot_glyph( G* g, int c, float x, float y, float w, float h, float a )
{
    select_region( c );
    set_drawing_scale( w / 10.0, h / 20.0 );
    set_drawing_angle( a );
    float ca = cos32( g, a );
    float sa = sin32( g, a );
    float ox = ( w / 2 ) * ca - ( h / 2 ) * sa;
    float oy = ( w / 2 ) * sa + ( h / 2 ) * ca;
    draw_region_rotozoomed_at( (int)( x - ox ), (int)( y - oy ) );
    set_drawing_angle( 0 );
    g->last_region = -1;
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

// decimal string of n in out (out must hold >= 9 chars: max 7 digits + 0)
void score_str( int n, char* out )
{
    char tmp[ 12 ];
    int i = 0;
    int len = 0;
    if( n <= 0 ) { out[ 0 ] = '0'; out[ 1 ] = 0; return; }
    while( n > 0 && i < 11 )
    {
        tmp[ i ] = '0' + n % 10;
        n /= 10;
        i++;
    }
    len = i;
    for( i = 0; i < len; i++ ) out[ i ] = tmp[ len - 1 - i ];
    out[ len ] = 0;
}

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
    float shapey = g->SHAPE_Y[ li ] + ( g->SHAPE_Y[ ln ] - g->SHAPE_Y[ li ] ) * f;
    // SHAPE scales the HORIZONTAL radius, SHAPE_Y the vertical one.
    // On the classic pass SHAPE_Y is all 1.0 — the shape reads as a
    // horizontally-modulated ellipse (the tuned look of levels 1-16,
    // and the reason the "square"/"triangle" webs never looked truly
    // square/triangular). On the true-geometry pass SHAPE_Y == SHAPE,
    // so the rim traces the actual polygon in both axes.
    float rx = lerp( OUT_RX, IN_RX, z ) * shape;
    float ry = lerp( OUT_RY, IN_RY, z ) * shapey;
    // level transition: uniform outward streaming (see fly_factor) —
    // everything scales together, so the tube never inverts
    float fly = fly_factor( g );
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

// ---------------------------------------------------------------------------
//  Audio — SFX round-robin over channels 2..13; channel 0 is reserved
//  for music, 1 is spare. Gameplay tracks play one after another (loop
//  OFF); the title theme loops. Channel state register: 0x40 stopped,
//  0x41 paused, 0x42 playing.
// ---------------------------------------------------------------------------
void sfx( G* g, int snd )
{
    if( g->sfx_channel < 2 || g->sfx_channel > 13 ) g->sfx_channel = 2;
    play_sound_in_channel( snd, g->sfx_channel );
    // play_sound_in_channel leaves this channel selected — apply the
    // SFX volume live so pause-menu changes reach every channel
    set_channel_volume( g->sfx_volume );
    g->sfx_channel++;
}

// play gameplay track n (0..3) on the music channel, loop OFF.
// NEVER stack: play_sound_in_channel only assigns + plays — on real
// hardware a Play command on an already-playing channel layers the
// new track over the old one instead of replacing it. So: STOP the
// channel first, then assign and play (note assign_channel_sound
// takes the CHANNEL id first, unlike play_sound_in_channel).
void play_track( G* g, int track )
{
    if( track > 3 ) track = 0;   // wrap: the cycle loops 1->2->3->4->1
    if( track < 0 ) track = 3;
    g->music_index = track;
    stop_channel( 0 );
    select_channel( 0 );
    set_channel_loop( 0 );   // loop OFF (0/1, not bool literals)
    assign_channel_sound( 0, M_TRACK1 + track );
    play_channel( 0 );
    set_channel_volume( g->music_volume );   // play_channel selected 0
}

// the 32 levels are split into 8-level MUSIC BANDS: track 1 plays on
// levels 1-8, track 2 on 9-16, track 3 on 17-24, track 4 on 25-32 —
// and past level 32 the bands wrap (4 -> 1), so an endless run never
// runs out of music. Within a level the tracks still hand off
// 1->2->3->4->1 as each one finishes (update_music); start_level
// re-stamps the band track whenever the level crosses a band edge.
int level_track( G* g )
{
    int t = ( ( g->level - 1 ) / 8 ) % 4;
    if( t < 0 ) t = 0;
    return t;
}

// title theme on the music channel, loop ON (same stop-first rule)
void play_title_music( G* g )
{
    stop_channel( 0 );
    select_channel( 0 );
    set_channel_loop( 1 );   // loop ON
    assign_channel_sound( 0, M_TITLE );
    play_channel( 0 );
    set_channel_volume( g->music_volume );
}

// gameplay track succession: roll to the next track when the current
// one finishes (called only during play/dying/transition/pause)
void update_music( G* g )
{
    if( get_channel_state( 0 ) == CH_STOPPED )
    {
        int t = g->music_index + 1;
        if( t > 3 ) t = 0;
        play_track( g, t );
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
            g->ENEMIES[ i ].dir = ( rng( g ) % 2 ) * 2 - 1;
            // walkers start their rim patrol aimed at the player
            if( type == 3 )
            {
                float d = g->player_lane - g->ENEMIES[ i ].lane;
                if( d > LANES / 2 ) d -= LANES;
                if( d < -LANES / 2 ) d += LANES;
                g->ENEMIES[ i ].dir = 1;
                if( d < 0 ) g->ENEMIES[ i ].dir = -1;
                g->ENEMIES[ i ].wig = 0;
            }
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
            g->ENEMIES[ i ].dir = ( rng( g ) % 2 ) * 2 - 1;
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

int count_walkers( G* g )
{
    int i; int n = 0;
    for( i = 0; i < MAX_ENEMIES; i++ )
        if( g->ENEMIES[ i ].alive && g->ENEMIES[ i ].type == 3 ) n++;
    return n;
}

// used by the busy-frame throttles (burst, starfield): particle count
int count_particles( G* g )
{
    int i; int n = 0;
    for( i = 0; i < MAX_PARTICLES; i++ ) if( g->PARTICLES[ i ].alive ) n++;
    return n;
}

void burst( G* g, float x, float y, int count, int strength )
{
    // BUSY-FRAME THROTTLE: the emulator's 100%-CPU stalls come from
    // exceeding the GPU draw-call budget, and every particle is one
    // draw. When lots are already flying (multi-kills, superzap
    // chains, death salvo) shrink NEW bursts instead of piling on —
    // the explosion still reads, calm frames get the full show.
    int alivep = count_particles( g );
    if( alivep > 160 ) count = count / 4;
    else if( alivep > 100 ) count = count / 2;
    else if( alivep > 60 ) count = count * 3 / 4;
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
    sfx( g, PEWPEW );
    // SUPER LASER: three shots at once — the claw's own lane plus the
    // two half-lanes either side of it, blanketing neighbouring rim
    // lanes. Slightly slower cycle than the single blaster.
    if( g->laser_timer > 0 )
    {
        g->fire_cooldown = 8;
        int k;
        for( k = -1; k <= 1; k++ )
        {
            float ln = g->player_lane + k * 1.5;
            if( ln < 0 ) ln += LANES;
            if( ln >= LANES ) ln -= LANES;
            int i;
            for( i = 0; i < MAX_BULLETS; i++ )
            {
                if( !g->BULLETS[ i ].alive )
                {
                    g->BULLETS[ i ].alive = 1;
                    g->BULLETS[ i ].lane = ln;
                    g->BULLETS[ i ].z = 0.02;
                    break;
                }
            }
        }
        return;
    }
    // RAPID BLASTER: 25% faster cycle — alternating 5/4-frame
    // cooldowns average 4.5 (the cooldown is an int, the rate isn't).
    // Spelled as if/else: a ternary here once leaked unrewritten into
    // the generated C ("character '?' is not a valid identifier
    // start") — the limitations doc tracks the transpiler gap.
    g->fire_cooldown = 6;
    if( g->rapid_timer > 0 )
    {
        g->fire_cooldown = 4;
        if( ( g->frame & 1 ) != 0 ) g->fire_cooldown = 5;
    }
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
    sfx( g, ZAPSND );
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

// claw movement, factored out of update_player so it can also run
// during the warp-out transition (dodge the spikes streaming past!)
void move_claw( G* g )
{
    if( gamepad_left() > 0 )  g->player_lane -= 0.09;
    if( gamepad_right() > 0 ) g->player_lane += 0.09;
    if( g->player_lane < 0 ) g->player_lane += LANES;
    if( g->player_lane >= LANES ) g->player_lane -= LANES;

    // OPEN WEBS: clamp the claw to its contiguous rim run so it can't
    // slide over a missing outline edge (see run_bounds). Fully closed
    // webs skip the clamp entirely — free movement all the way around.
    // While AIRBORNE (jump) the clamp is suspended — leaping a rim gap
    // is the one way to cross between a web's separated arcs.
    // The run is looked up from the NEAREST lane, not the truncated
    // one: a claw at 8.91 sits in the (8,9) gap and must fall back to
    // lane 9's run — with truncation it clamped into the OTHER arc
    // (the cross-half teleport bug).
    if( !web_full( g ) && g->jump_timer <= 0 )
    {
        int plo; int phi;
        int pli = (int)( g->player_lane + 0.5 );
        if( pli >= LANES ) pli -= LANES;
        run_bounds( g, pli, &plo, &phi );
        if( plo == phi ) g->player_lane = plo;               // single-lane run
        else if( plo < phi )
        {
            if( g->player_lane < plo ) g->player_lane = plo;
            if( g->player_lane > phi ) g->player_lane = phi;
        }
        else  // wrapped run [plo..15]+[0..phi]: the gap zone is the
              // open interval (phi,plo) — ONLY positions there get
              // clamped; everything else (including the 15->0 seam
              // crossing) is legal run space. Snap back to whichever
              // END of the gap the claw just left (nearest by run
              // midpoint) — never across it.
        {
            if( g->player_lane > phi && g->player_lane < plo )
            {
                float mid = ( plo + phi ) * 0.5;
                if( g->player_lane < mid ) g->player_lane = phi;
                else                       g->player_lane = plo;
            }
        }
    }

    // camera sway: ease the vanishing point toward a fraction of the
    // claw's offset from the tube axis — the tube appears to lean and
    // follow you around the web (Tempest 2000 flavour)
    project( g, g->player_lane, 0 );
    float camtx = ( g->px - CX ) * CAM_SWAY;
    float camty = ( g->py - CY ) * CAM_SWAY;
    g->cam_x += ( camtx - g->cam_x ) * CAM_EASE;
    g->cam_y += ( camty - g->cam_y ) * CAM_EASE;
}

// JUMP (button Y), factored out so it also runs during the warp-out:
// the claw leaps off the web toward the camera — airborne frames are
// invulnerable at the rim, so you can hop over a camper, or over a
// spike sweeping past during the level transition
void claw_jump( G* g )
{
    if( g->jump_cooldown > 0 ) g->jump_cooldown--;
    if( g->jump_timer > 0 ) g->jump_timer--;
    else if( gamepad_button_y() == 1 && g->jump_cooldown == 0 )
    {
        g->jump_timer = 34;
        g->jump_cooldown = 60;   // lands at 34, 26 frames of recovery
        burst( g, g->px, g->py, 8, 1.5 );   // takeoff puff
        sfx( g, JUMP );
    }
}

void update_player( G* g )
{
    move_claw( g );
    claw_jump( g );
    if( g->laser_timer > 0 ) g->laser_timer--;
    if( g->rapid_timer > 0 ) g->rapid_timer--;
    if( gamepad_button_a() > 0 ) fire( g );
    if( gamepad_button_b() == 1 ) superzap( g );   // == 1: just-pressed edge
    if( g->fire_cooldown > 0 ) g->fire_cooldown--;
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
    sfx( g, DEATH );
}

void level_clear( G* g );

// ---------------------------------------------------------------------------
//  Power-ups: shoot the pods to collect them.
//    type 0 = SUPERZAP refill, type 1 = AI buddy, type 2 = extra life,
//    type 3 = SUPER LASER (triple-shot for a timed stretch),
//    type 4 = RAPID BLASTER (25% faster single shots; easy = unlimited)
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
        // easy keeps the buddy around much longer
        int secs = 15;
        if( g->difficulty == 0 ) secs = 30;
        if( g->difficulty == 2 ) secs = 10;
        g->buddy_timer = secs * 60;
        g->buddy_lane = g->player_lane;
        show_message( g, "AI BUDDY ONLINE!" );
    }
    else if( g->POWERUPS[ i ].type == 3 )
    {
        int secs = 12;
        if( g->difficulty == 0 ) secs = 20;
        if( g->difficulty == 1 ) secs = 15;
        g->laser_timer = secs * 60;
        show_message( g, "SUPER LASER!" );
    }
    else if( g->POWERUPS[ i ].type == 4 )
    {
        // intermediate blaster: 25% faster single shots; easy never
        // runs out (999999 frames is ~4.6 hours of play)
        if( g->difficulty == 0 )      g->rapid_timer = 999999;
        else if( g->difficulty == 1 ) g->rapid_timer = 60 * 60;
        else                          g->rapid_timer = 30 * 60;
        show_message( g, "RAPID BLASTER!" );
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
    sfx( g, PICKUP );
    g->POWERUPS[ i ].alive = 0;
}

void spawn_powerup( G* g )
{
    int i;
    for( i = 0; i < 4; i++ )
    {
        if( g->POWERUPS[ i ].alive ) continue;
        g->POWERUPS[ i ].alive = 1;
        // open webs: pods spawn inside the player's contiguous run so
        // they are always reachable (closed webs: any lane)
        if( web_full( g ) )
        {
            g->POWERUPS[ i ].lane = rng( g ) % LANES;
        }
        else
        {
            int plo; int phi;
            int pli = (int)g->player_lane;
            if( pli >= LANES ) pli -= LANES;
            run_bounds( g, pli, &plo, &phi );
            int span = phi - plo + 1;
            if( span <= 0 ) span += LANES;   // wrapped run
            g->POWERUPS[ i ].lane = plo + rng( g ) % span;
            if( g->POWERUPS[ i ].lane >= LANES ) g->POWERUPS[ i ].lane -= LANES;
        }
        g->POWERUPS[ i ].z = 1.0;
        int r = rng( g ) % 20;
        if( r < 8 )       g->POWERUPS[ i ].type = 0;   // superzap
        else if( r < 13 ) g->POWERUPS[ i ].type = 1;   // AI buddy
        else if( r < 16 ) g->POWERUPS[ i ].type = 4;   // rapid blaster
        else if( r < 18 ) g->POWERUPS[ i ].type = 3;   // super laser
        else              g->POWERUPS[ i ].type = 2;   // extra life
        // don't offer superzap refills when the player is already at
        // the 4-charge cap — a wasted pod. Convert to the intermediate
        // rapid blaster instead (super laser stays rare).
        if( g->POWERUPS[ i ].type == 0 && g->superzaps >= 4 )
            g->POWERUPS[ i ].type = 4;
        // hard mode: no extra lives handed out — becomes a buddy
        if( g->POWERUPS[ i ].type == 2 && g->difficulty == 2 )
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
        if( g->difficulty == 0 )      g->powerup_timer = 420 + rng( g ) % 500;
        else if( g->difficulty == 2 ) g->powerup_timer = 1300 + rng( g ) % 900;
        else                          g->powerup_timer = 700 + rng( g ) % 700;
        int cap = 2;
        if( g->difficulty == 0 ) cap = 3;   // easy: pods stack up
        if( g->difficulty == 2 ) cap = 1;   // hard: one at a time
        int n = 0;
        for( i = 0; i < 4; i++ ) if( g->POWERUPS[ i ].alive ) n++;
        if( n < cap ) spawn_powerup( g );
    }
}

// AI buddy: hovers by the claw, auto-fires at the enemy closest to the
// player's lane every 30 frames while its timer lasts
// AI buddy: an autonomous drone with its OWN lane — it swoops toward the
// enemy nearest the player and fires FROM WHERE IT VISIBLY IS, once
// aligned with the target. (Previously bullets spawned on the TARGET's
// lane while the drone hovered beside the claw — shots looked like they
// came from a completely different part of the web.)
void update_buddy( G* g )
{
    if( g->buddy_timer <= 0 ) return;
    g->buddy_timer--;
    if( g->buddy_cooldown > 0 ) g->buddy_cooldown--;

    // pick the target: enemy closest to the PLAYER's lane
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

    // ease toward the target's lane (wrap-aware shortest arc); with no
    // enemies alive, fall back to hovering beside the claw
    float want = g->player_lane + 1.9;
    if( best >= 0 ) want = g->ENEMIES[ best ].lane;
    float d = want - g->buddy_lane;
    while( d > LANES / 2 ) d -= LANES;
    while( d < -LANES / 2 ) d += LANES;
    g->buddy_lane += d * 0.2;
    if( g->buddy_lane < 0 ) g->buddy_lane += LANES;
    if( g->buddy_lane >= LANES ) g->buddy_lane -= LANES;

    // fire only once ALIGNED, from the drone's own position
    if( best >= 0 && g->buddy_cooldown <= 0 )
    {
        float ad = g->ENEMIES[ best ].lane - g->buddy_lane;
        while( ad > LANES / 2 ) ad -= LANES;
        while( ad < -LANES / 2 ) ad += LANES;
        if( ad < 0 ) ad = -ad;
        if( ad < 0.6 )
        {
            int j;
            for( j = 0; j < MAX_BULLETS; j++ )
            {
                if( !g->BULLETS[ j ].alive )
                {
                    g->BULLETS[ j ].alive = 1;
                    g->BULLETS[ j ].lane = g->buddy_lane;
                    g->BULLETS[ j ].z = 0.10;
                    g->buddy_cooldown = 30;
                    break;
                }
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
        if( g->difficulty == 0 ) speed *= 0.8;
        if( g->difficulty == 2 ) speed *= 1.25;

        // spiker: patrols mid-tunnel laying spikes
        if( g->ENEMIES[ i ].type == 2 )
        {
            if( g->ENEMIES[ i ].cooldown > 0 ) g->ENEMIES[ i ].cooldown--;
            else
            {
                // patrol with a direction; BOUNCE at missing rim edges
                // (open webs) so spikes never appear on dead lanes
                int sli = (int)g->ENEMIES[ i ].lane;
                if( sli >= LANES ) sli -= LANES;
                int ahead = sli;
                int back = sli - 1; if( back < 0 ) back += LANES;
                int blocked_ahead = !g->CONN[ ahead ];
                int blocked_back = !g->CONN[ back ];
                if( g->ENEMIES[ i ].dir > 0 && blocked_ahead ) g->ENEMIES[ i ].dir = -1;
                if( g->ENEMIES[ i ].dir < 0 && blocked_back ) g->ENEMIES[ i ].dir = 1;
                int canmove = 1;
                if( g->ENEMIES[ i ].dir > 0 && blocked_ahead ) canmove = 0;
                if( g->ENEMIES[ i ].dir < 0 && blocked_back ) canmove = 0;
                if( canmove )
                {
                    g->ENEMIES[ i ].lane += g->ENEMIES[ i ].dir;
                    if( g->ENEMIES[ i ].lane < 0 ) g->ENEMIES[ i ].lane += LANES;
                    if( g->ENEMIES[ i ].lane >= LANES ) g->ENEMIES[ i ].lane -= LANES;
                }
                g->ENEMIES[ i ].cooldown = 26;
            }
            int li = ( (int)g->ENEMIES[ i ].lane ) % LANES;
            if( g->SPIKE[ li ] < 0.55 ) g->SPIKE[ li ] += 0.006;
            continue;
        }

        // WALKER ('X'): climbs the tube like a flipper; once at the rim
        // it WALKS the outer edge lane by lane while tumbling
        // CONTINUOUSLY end-over-end (a stepped 180-deg flip only at
        // each lane change read as snapping spoke-to-spoke), reversing
        // at rim gaps. Lethal on lane contact like any rim camper —
        // hop it or shoot it.
        if( g->ENEMIES[ i ].type == 3 )
        {
            if( g->ENEMIES[ i ].z > 0 )
            {
                g->ENEMIES[ i ].z -= speed;
                if( g->ENEMIES[ i ].z < 0 ) g->ENEMIES[ i ].z = 0;
                g->ENEMIES[ i ].wig += 2;      // lazy spin on the climb
            }
            else
            {
                g->ENEMIES[ i ].wig += 5;      // fast end-over-end tumble
                float diffw = g->player_lane - g->ENEMIES[ i ].lane;
                if( diffw < 0 ) diffw = -diffw;
                if( diffw > LANES / 2 ) diffw = LANES - diffw;
                if( diffw < 0.7 && g->jump_timer <= 0 ) { kill_player( g ); }

                if( g->ENEMIES[ i ].cooldown > 0 ) g->ENEMIES[ i ].cooldown--;
                else
                {
                    int wli = (int)g->ENEMIES[ i ].lane;
                    if( wli >= LANES ) wli -= LANES;
                    int ahead = wli;                    // edge wli -> wli+1
                    int back = wli - 1; if( back < 0 ) back += LANES;
                    if( g->ENEMIES[ i ].dir > 0 && !g->CONN[ ahead ] )
                        g->ENEMIES[ i ].dir = -1;
                    if( g->ENEMIES[ i ].dir < 0 && !g->CONN[ back ] )
                        g->ENEMIES[ i ].dir = 1;
                    int canmove = 1;
                    if( g->ENEMIES[ i ].dir > 0 && !g->CONN[ ahead ] ) canmove = 0;
                    if( g->ENEMIES[ i ].dir < 0 && !g->CONN[ back ] ) canmove = 0;
                    if( canmove )
                    {
                        g->ENEMIES[ i ].lane += g->ENEMIES[ i ].dir;
                        if( g->ENEMIES[ i ].lane < 0 ) g->ENEMIES[ i ].lane += LANES;
                        if( g->ENEMIES[ i ].lane >= LANES ) g->ENEMIES[ i ].lane -= LANES;
                    }
                    // patrol pace quickens with level & difficulty
                    int stepf = 22 - g->level / 4;
                    if( g->difficulty == 0 ) stepf += 6;
                    if( g->difficulty == 2 ) stepf -= 4;
                    if( stepf < 8 ) stepf = 8;
                    g->ENEMIES[ i ].cooldown = stepf;
                }
            }
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
                // open webs: a flipper cannot cross a missing rim edge —
                // gate the lane switch on CONN (it just camps at the end)
                if( diff > 0.5 )
                {
                    int fli = (int)g->ENEMIES[ i ].lane;
                    if( fli >= LANES ) fli -= LANES;
                    if( g->CONN[ fli ] ) g->ENEMIES[ i ].lane += 1;
                }
                else if( diff < -0.5 )
                {
                    int fli = (int)g->ENEMIES[ i ].lane - 1;
                    if( fli < 0 ) fli += LANES;
                    if( g->CONN[ fli ] ) g->ENEMIES[ i ].lane -= 1;
                }
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
                sfx( g, BOOM );

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
    g->state_timer = 160;     // exactly 160 * 0.0125 = warp 2.0: fly-out + fly-in
    g->warp_phase = 0;
    g->warp_bounce = 0;       // fresh exit: spikes CAN reject you (once)
    g->warp = 0;
    show_message( g, "EXCELLENT!" );
    sfx( g, CLEAR );
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
        float dx = g->STARFIELD[ i ].x - ox;
        float dy = g->STARFIELD[ i ].y - oy;
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
            g->STARFIELD[ i ].x = ox + cos32( g, a ) * r;
            g->STARFIELD[ i ].y = oy + sin32( g, a ) * r;
            g->STARFIELD[ i ].spd = 0.8 + frand( g ) * 1.6;
        }
    }
}

void render_starfield( G* g )
{
    int i;
    set_blending_mode( BLEND_SOLID );
    // BUSY-FRAME BUDGET: when explosions fill the sky, halve the star
    // draws — background garnish is the cheapest thing to shed, and
    // the streaking motion hides the thinning
    int starstep = 1;
    if( count_particles( g ) > 120 ) starstep = 2;
    for( i = 0; i < MAX_STARS; i += starstep )
    {
        // fast/near stars: big and white; slow/far: smaller, cool blue
        if( g->STARFIELD[ i ].spd > 1.6 )
        {
            set_multiply_color( make_color( 240, 245, 255 ) );
            draw_glyph( g, '.', g->STARFIELD[ i ].x, g->STARFIELD[ i ].y, 5, 5 );
        }
        else
        {
            set_multiply_color( make_color( 150, 165, 215 ) );
            draw_glyph( g, '.', g->STARFIELD[ i ].x, g->STARFIELD[ i ].y, 3, 3 );
        }
    }
}

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
                float wv = sin32( g, colg * 0.5 + t );
                int reg = 17;
                if( wv > 0.55 )       reg = 20;   // crest: solid
                else if( wv > 0.0 )   reg = 19;
                else if( wv > -0.55 ) reg = 18;
                if( reg != creg ) { select_region( reg ); creg = reg; }
                float bx = 56 + i * 48 + c * 8 + wv * 5.0;
                float by = 48 + r * 8 + sin32( g, colg * 0.28 + t * 0.7 ) * 3.5;
                draw_region_zoomed_at( (int)( bx ), (int)( by ) );
            }
        }
        i++;
    }
    // raw region draws invalidate the draw_glyph cache
    g->last_region = -1;
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;

    // menu — stacked, centered; every option undulates, the selection
    // gets the bigger wave + hue cycle. Row 3 shows the difficulty;
    // LEFT/RIGHT changes it while that row is selected.
    char* rows[ 4 ];
    rows[ 0 ] = "PLAY";
    rows[ 1 ] = "HIGH SCORES";
    rows[ 2 ] = "LEVEL SELECT";
    if( g->difficulty == 0 )      rows[ 3 ] = "DIFFICULTY: EASY";
    else if( g->difficulty == 1 ) rows[ 3 ] = "DIFFICULTY: MEDIUM";
    else                          rows[ 3 ] = "DIFFICULTY: HARD";
    int d; int ci;
    for( d = 0; d < 4; d++ )
    {
        float sel = 0;
        if( d == g->menu_row ) sel = 1;
        float msz = 14 + sel * 4;
        // selected option gets wider letter spacing (0.78 vs 0.62)
        float adv = msz * 0.62;
        if( sel > 0 ) adv = msz * 0.78;
        ci = 0;
        while( rows[ d ][ ci ] != 0 )
        {
            float wob = sin32( g, ci * 0.6 + g->frame * 0.04 + d ) * ( 2 + sel * 4 );
            if( d == g->menu_row )
                set_multiply_color( hue( g->frame + ci * 14 ) );
            else
                set_multiply_color( make_color( 90, 100, 125 ) );
            draw_glyph( g, rows[ d ][ ci ],
                        320 + ( ci - ( string_len( rows[ d ] ) - 1 ) / 2.0 ) * adv,
                        150 + d * 28 + wob, msz, msz );
            ci++;
        }
    }

    // gameplay controls legend (12px: 10px text clipped its glyph rows)
    draw_text( g, "A: FIRE  B: SUPERZAP  Y: JUMP",
               212, 268, 12, make_color( 120, 130, 160 ) );

    // controls hint
    draw_text( g, "UP/DOWN: SELECT  LEFT/RIGHT: DIFFICULTY  A: GO",
               149, 296, 12, make_color( 120, 130, 160 ) );
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
        if( !g->CONN[ i ] ) continue;   // open web: cap mirrors the rim gap
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
    // budget treatment). One dot per lane per ring. BUSY-FRAME CULL:
    // 7 rings x 16 lanes = 112 draws/frame, second only to particles.
    // During heavy explosion salvos halve the dots (every other lane);
    // the rings read fine sparse and the budget goes to the fireworks.
    int ringstep = 1;
    if( count_particles( g ) > 120 ) ringstep = 2;
    float scroll = 0.002 + g->warp * g->warp * 0.12;
    for( r = 0; r < RINGS; r++ )
    {
        float z = ( ( g->frame * scroll ) + r / ( RINGS * 1.0 ) );
        while( z > 1 ) z -= 1;
        set_multiply_color( make_color( 20, 60 + r * 6, 90 ) );
        for( i = 0; i < LANES; i += ringstep )
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
        if( !g->CONN[ i ] ) continue;   // open web: no bar over the gap
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
    // warp-out — it parks on the rim while the web streams past it.
    // project() has the fly factor baked in, so divide it back out.
    project( g, g->player_lane, 0 );
    float x = g->px; float y = g->py; float s = g->pscale;
    float fly = fly_factor( g );
    x = CX + ( x - CX ) / fly;
    y = CY + ( y - CY ) / fly;

    // jump arc: airborne claw pops toward the camera — bigger and
    // pushed radially outward from the tube axis (T2K-style leap)
    if( g->jump_timer > 0 )
    {
        float jarc = sin32( g, 3.14159 * ( 34 - g->jump_timer ) / 34.0 );
        x += ( x - CX ) * jarc * 0.18;
        y += ( y - CY ) * jarc * 0.18;
        s *= ( 1.0 + jarc * 1.2 );
    }

    // THE CLAW IS BIOS REGION 123 — the left-curly-brace glyph: its aims
    // down the lane spoke — the exact direction the shots travel — so
    // the ship visibly points where it fires. aim = rim -> far-cap.
    project( g, g->player_lane, 1 );
    float capx = CX + ( g->px - CX ) / fly;
    float capy = CY + ( g->py - CY ) / fly;
    float aim = dir_angle( g, capx - x, capy - y ) * 0.024543692;

    set_blending_mode( BLEND_SOLID );
    set_multiply_color( make_color( 255, 220, 60 ) );
    draw_rot_glyph( g, 123, x, y, 24 * s, 30 * s, aim );
    // hot core, glowing, riding the same rotation
    set_glow( 3 );
    set_multiply_color( make_color( 255, 120, 40 ) );
    draw_rot_glyph( g, 123, x, y, 12 * s, 18 * s, aim );
    set_blending_mode( BLEND_SOLID );
}

void render_enemies( G* g )
{
    // BUSY-FRAME CULL: each enemy's glow core is a second draw per
    // enemy. During heavy explosion salvos skip them — the body glyph
    // still reads, and each iteration resets blending to solid at the
    // top so a skipped core never leaks state into the next enemy.
    int busyp = count_particles( g );
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
            set_multiply_color( make_color( 255, 80, 160 ) );
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
            set_multiply_color( make_color( 255, 70, 70 ) );
            float rot = ( g->ENEMIES[ i ].wig & 255 ) * 0.024543692;
            draw_rot_glyph( g, 'X', x, y, 22 * sv, 28 * sv, rot );
        }
        else
        {
            set_multiply_color( make_color( 255, 150, 40 ) );
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
        set_multiply_color( make_color( 255, 200 - (int)( g->BULLETS[ i ].z * 180 ),
                                       120 - (int)( g->BULLETS[ i ].z * 100 ) ) );
        // normal shots are '.', rapid-blaster shots a fatter '*'
        int bg = '.';
        float bs = 7 * s + 3;
        if( g->rapid_timer > 0 ) { bg = '*'; bs = 12 * s + 4; }
        draw_glyph( g, bg, g->px, g->py, bs, bs );
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
        else if( g->POWERUPS[ i ].type == 3 ) col = make_color( 80, 220, 255 );
        else if( g->POWERUPS[ i ].type == 4 ) col = make_color( 255, 160, 60 );
        else                                  col = make_color( 255, 220, 60 );
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
    set_multiply_color( make_color( 120, 255, 160 ) );
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

    // lives as spare claws: the brace ship (region 123), aimed at the
    // web's centre so the icons echo the player sprite ('X' now
    // belongs to the walker)
    int l;
    set_blending_mode( BLEND_SOLID );
    set_multiply_color( make_color( 255, 220, 60 ) );
    for( l = 0; l < g->lives; l++ )
    {
        float ix = 560 + l * 22;
        float iy = 344;
        int aimidx = dir_angle( g, CX - ix, CY - iy );
        draw_rot_glyph( g, 123, ix, iy, 12, 16, aimidx * 0.024543692 );
    }

    // level
    char lvl[ 7 ];
    lvl[ 0 ] = 'L'; lvl[ 1 ] = 'V'; lvl[ 2 ] = 'L'; lvl[ 3 ] = ' ';
    lvl[ 4 ] = '0' + ( g->level / 10 ) % 10;
    lvl[ 5 ] = '0' + g->level % 10;
    lvl[ 6 ] = 0;
    draw_text( g, lvl, 20, 344, 14, make_color( 120, 200, 255 ) );

    // superzapper charges — bottom row spread out so the pod timers
    // (AI / RAPID / LASER) never crowd them or each other
    set_multiply_color( make_color( 255, 255, 255 ) );
    for( l = 0; l < g->superzaps; l++ )
        draw_glyph( g, 'Z', 240 + l * 20, 344, 14, 14 );

    // AI buddy countdown (seconds remaining) while it is online
    if( g->buddy_timer > 0 )
    {
        char bud[ 6 ];
        int secs = g->buddy_timer / 60;
        bud[ 0 ] = 'A'; bud[ 1 ] = 'I'; bud[ 2 ] = ' ';
        bud[ 3 ] = '0' + ( secs / 10 ) % 10;
        bud[ 4 ] = '0' + secs % 10;
        bud[ 5 ] = 0;
        draw_text( g, bud, 316, 344, 14, make_color( 120, 255, 160 ) );
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
            draw_text( g, rap, 386, 344, 14, make_color( 255, 160, 60 ) );
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
            draw_text( g, las, 470, 344, 14, make_color( 80, 220, 255 ) );
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
        if( s < segs ) set_multiply_color( make_color( 90, 220, 120 ) );
        else          set_multiply_color( make_color( 30, 45, 40 ) );
        draw_region_zoomed_at( x + s * 22, y );
    }
    g->last_region = -1;
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;
}

void render_pause( G* g )
{
    set_blending_mode( v32::BlendAlpha );
    select_region( 20 );
    set_drawing_scale( 64.0, 16.8 );          // 640 x 336 fullscreen veil
    set_multiply_color( make_color( 8, 8, 26 ) );
    draw_region_zoomed_at( 0, 0 );
    g->last_region = -1;
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;
    set_blending_mode( BLEND_SOLID );

    draw_party_text( g, "PAUSED", 320, 44, 22, 90 );

    // separate meters: MUSIC (channel 0) and SFX (channels 2..13)
    draw_text( g, "MUSIC", 240, 88, 14, make_color( 150, 160, 190 ) );
    draw_meter( g, 220, 112, g->music_volume );
    draw_text( g, "SFX", 240, 144, 14, make_color( 150, 160, 190 ) );
    draw_meter( g, 220, 168, g->sfx_volume );

    // track list
    draw_text( g, "TRACK", 240, 200, 14, make_color( 150, 160, 190 ) );
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
        else                      colr = make_color( 90, 100, 125 );
        draw_text( g, tracks[ t ], 250, 224 + t * 20, 14, colr );
    }

    draw_text( g, "START: RESUME  LEFT/RIGHT: MUSIC  L/R: SFX  UP/DOWN: TRACK",
               68, 310, 14, make_color( 120, 130, 160 ) );
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
        int colr = make_color( 90, 100, 125 );
        if( i == g->entry_pos ) colr = hue( g->frame );
        draw_text( g, &ch[ 0 ], 320 + ( i - 1 ) * 56 - 10, 180, 30, colr );
    }
    draw_text( g, "ENTER YOUR INITIALS", 249, 160, 12, make_color( 120, 130, 160 ) );
    draw_text( g, "UP/DOWN: LETTER  LEFT/RIGHT: MOVE  A: OK",
               171, 280, 12, make_color( 120, 130, 160 ) );
}

// state 7: the stored table, top 5
void render_scores( G* g )
{
    draw_party_text( g, "HIGH SCORES", 320, 56, 20, 80 );
    int i;
    for( i = 0; i < 5; i++ )
    {
        int y = 130 + i * 32;
        int colr = make_color( 120, 130, 160 );
        if( i == 0 ) colr = hue( g->frame );
        char rk[ 3 ];
        rk[ 0 ] = '1' + i;
        rk[ 1 ] = '.';
        rk[ 2 ] = 0;
        draw_text( g, &rk[ 0 ], 246, y, 14, colr );
        // initials: drawn one letter at a time with a WIDER advance —
        // the plain row advance (size * 0.62) crams 14px glyphs into
        // 8.7px slots and the letters read as a scrunched block
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
    draw_text( g, "A: BACK", 294, 330, 12, make_color( 120, 130, 160 ) );
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
    set_multiply_color( make_color( 90, 100, 130 ) );
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
               190, 330, 12, make_color( 120, 130, 160 ) );
}

// ---------------------------------------------------------------------------
//  Level flow
// ---------------------------------------------------------------------------
void start_level( G* g )
{
    int i;
    for( i = 0; i < MAX_ENEMIES; i++ ) g->ENEMIES[ i ].alive = 0;
    for( i = 0; i < MAX_BULLETS; i++ ) g->BULLETS[ i ].alive = 0;
    int zaps = 2;
    if( g->difficulty == 0 ) zaps = 3;
    if( g->difficulty == 2 ) zaps = 1;
    g->superzaps = zaps;
    for( i = 0; i < LANES; i++ ) g->SPIKE[ i ] = 0;
    g->launched = 0;
    make_shape( g );
    // crossed a music band edge (or respawn)? stamp the band's track —
    // if the current track already IS the band track this is a no-op
    int bt = level_track( g );
    if( bt != g->music_index ) play_track( g, bt );
    g->warp = 0;              // insurance: no transition residue in play
    g->warp_phase = 0;
    g->warp_bounce = 0;
    g->spawn_interval = 70 - g->level * 2;
    // difficulty pacing: easy breathes, hard floods
    if( g->difficulty == 0 ) g->spawn_interval += 14;
    if( g->difficulty == 2 ) g->spawn_interval -= 12;
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
        // WALKERS ('X'): a rim patrol that tumbles end-over-end —
        // max two at once; they are persistent lane hazards
        int walker = ( roll == 2 && count_walkers( g ) < 2 );
        // if/else, not a nested ternary: the ternary rewrite leaked a
        // raw '?' into the generated C (transpiler gap, see doc)
        int etype = 0;
        if( spiker ) etype = 2;
        else if( walker ) etype = 3;
        else if( tanker ) etype = 1;
        spawn_enemy( g, etype );
        int quota = 18 + g->level;
        if( g->difficulty == 0 ) quota = 14 + g->level;
        if( g->difficulty == 2 ) quota = 23 + g->level;
        g->launched++;
        if( g->launched >= quota )
        {
            g->spawn_timer = 999999;   // stop spawning; level ends when clear
            g->launched = 0;
        }
    }
}

// ---------------------------------------------------------------------------
//  High scores — persisted on the Vircon32 memory card (raw layout; the
//  card is a flat 32K-BYTE space = 8192 WORDS, and ALL memcard offsets
//  and sizes are in WORDS — see the SDK's card_read_data asm: "movs"
//  copies CR words; card_read_signature uses CR=20 for int[20]. Mixing
//  in byte counts overflows the destination and SMASHES THE STACK —
//  a 20-word buffer fed size=80 blew 60 words of stack, corrupting
//  the return address: instant silent CPU halt at boot):
//    words  0..19 : game signature, 20 words spelling "TEMPEST32KHS"
//    words 20..24 : HISCORE[5]
//    words 25..39 : HIINIT[5][3] as character codes
//  A card whose signature doesn't match is treated as empty (defaults),
//  and the first save claims the area. No card = defaults, saves skipped.
//  Only card_is_connected/card_read_data/card_write_data are used, all
//  with int* arguments — card_signature_matches' typedef'd pointer type
//  (int[20]*) is deliberately avoided.
// ---------------------------------------------------------------------------
void hs_make_sig( char* tag, int* sig )
{
    int i;
    for( i = 0; i < 20; i++ ) sig[ i ] = 0;
    i = 0;
    while( tag[ i ] != 0 && i < 20 ) { sig[ i ] = tag[ i ]; i++; }
}

void hs_load( G* g )
{
    int i; int j;
    for( i = 0; i < 5; i++ )
    {
        g->HISCORE[ i ] = 0;
        for( j = 0; j < 3; j++ ) g->HIINIT[ i ][ j ] = '-';
    }
    if( !card_is_connected() ) return;
    int tag[ 20 ];
    int got[ 20 ];
    hs_make_sig( "TEMPEST32KHS", &tag[ 0 ] );
    card_read_data( &got[ 0 ], 0, 20 );          // 20 WORDS, not bytes!
    for( i = 0; i < 20; i++ ) if( got[ i ] != tag[ i ] ) return;
    card_read_data( &g->HISCORE[ 0 ], 20, 5 );       // words 20..24
    card_read_data( &g->HIINIT[ 0 ][ 0 ], 25, 15 );  // words 25..39
}

void hs_save( G* g )
{
    if( !card_is_connected() ) return;
    int tag[ 20 ];
    hs_make_sig( "TEMPEST32KHS", &tag[ 0 ] );
    card_write_data( &tag[ 0 ], 0, 20 );
    card_write_data( &g->HISCORE[ 0 ], 20, 5 );
    card_write_data( &g->HIINIT[ 0 ][ 0 ], 25, 15 );
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
    g->warp_phase = 0;
    g->warp_bounce = 0;
    g->frame = 0;
    g->rng_state = 12345;
    g->difficulty = 1;
    g->menu_cooldown = 0;
    g->state = 4;          // boot straight to the title screen
    g->message_timer = 0;
    g->message[ 0 ] = 0;
    g->last_region = -1;
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;

    // effects + power-up state: heap memory from `new` is NOT zeroed,
    // so every slot must be explicitly cleared here
    g->buddy_timer = 0;
    g->buddy_cooldown = 0;
    g->buddy_lane = 0;
    g->laser_timer = 0;
    g->rapid_timer = 0;
    g->powerup_timer = 500;
    g->jump_timer = 0;
    g->jump_cooldown = 0;
    g->music_index = 0;
    g->sfx_channel = 2;
    g->music_volume = 0.8;
    g->sfx_volume = 0.8;
    g->menu_row = 0;
    g->select_level = 1;
    g->hs_rank = -1;
    g->entry_pos = 0;
    g->entry_letters[ 0 ] = 'A';
    g->entry_letters[ 1 ] = 'A';
    g->entry_letters[ 2 ] = 'A';
    int i;
    for( i = 0; i < LANES; i++ ) g->CONN[ i ] = 1;   // closed until make_shape
    for( i = 0; i < 3; i++ ) g->SHOCKS[ i ].alive = 0;
    for( i = 0; i < 4; i++ ) g->POWERUPS[ i ].alive = 0;
    build_tables( g );
    init_starfield( g );
    hs_load( g );
}

void main()
{
    select_texture( -1 );           // BIOS font
    select_gamepad( 0 );

    // ALL mutable state lives on the heap: file-scope variables in
    // generated Vircon32 C are read-only (no `global` keyword emitted)
    G* g = new G;
    init_state( g );          // sets state 4 = title screen; heap-safe boot

    while( 1 )
    {
        // -- update ---------------------------------------------------------
        if( g->state == 0 )
        {
            if( gamepad_button_start() == 1 ) g->state = 5;   // pause
            else
            {
                update_player( g );
                update_spawning( g );
                update_enemies( g );
                update_bullets( g );
                update_powerups( g );
                update_buddy( g );
            }
        }
        else if( g->state == 1 )      // dying
        {
            g->state_timer--;
            if( g->state_timer <= 0 )
            {
                if( g->lives <= 0 )
                {
                    g->state = 3;
                    show_message( g, "GAME OVER" );
                    g->state_timer = 300;
                    play_title_music( g );   // gameplay succession ends here
                }
                else start_level( g );
            }
        }
        else if( g->state == 2 )      // warp-out: web flies past the claw
        {
            // two phases: A — the old web streams outward off-screen;
            // B — the next level's web shrinks back in to settle.
            // At the halfway point the level number and silhouette
            // switch, so what flies IN is the new level.
            g->warp += 0.0125;
            // the claw stays live during the fly-out: you can slide
            // between lanes and JUMP — the old web's spikes stream
            // outward with it and sweep past the claw plane
            move_claw( g );
            claw_jump( g );
            // SPIKE SWEEP (phase A): as the old web streams outward, the
            // tip of any spike sweeps past the claw plane. The crossing
            // is edge-detected per lane from fly_factor (a pure function
            // of warp), so dodging into a DIFFERENT spiked lane still
            // gets you at its own crossing frame — and a jump clears it
            // cleanly, since airborne frames are invulnerable.
            //   EASY        — deflected: replay the same level
            //   MEDIUM/HARD — killed: lose a life AND replay the level
            if( g->warp_phase == 0 && g->warp_bounce == 0 )
            {
                int li = (int)g->player_lane;
                if( li >= LANES ) li -= LANES;
                if( g->SPIKE[ li ] > 0.05 && g->jump_timer <= 0 )
                {
                    float ztip = 1.0 - g->SPIKE[ li ];
                    float rad = lerp( OUT_RX, IN_RX, ztip );
                    float wprev = g->warp - 0.0125;
                    if( wprev < 0 ) wprev = 0;
                    float fprev = 1.0 + wprev * wprev * 12.0;
                    if( rad * fly_factor( g ) >= OUT_RX &&
                        rad * fprev < OUT_RX )
                    {
                        project( g, g->player_lane, 0 );
                        if( g->difficulty == 0 )
                        {
                            g->warp_bounce = 1;
                            explode( g, g->px, g->py, 30, 3 );
                            shockwave( g, g->px, g->py );
                            show_message( g, "SPIKED! REPLAY LEVEL" );
                            sfx( g, BOOM );
                        }
                        else
                        {
                            // kill, but do NOT advance the level —
                            // start_level respawns on the same web
                            g->warp = 0;
                            g->warp_phase = 0;
                            kill_player( g );
                            show_message( g, "SPIKED!" );
                        }
                    }
                }
            }
            if( g->warp >= 1.0 && g->warp_phase == 0 )
            {
                if( g->warp_bounce == 0 )
                {
                    g->level++;
                    make_shape( g );
                }
                else g->warp_bounce = 0;   // bounced: same web flies back in
                g->warp_phase = 1;
            }
            g->state_timer--;
            if( g->state_timer <= 0 )
            {
                g->warp = 0;
                start_level( g );   // warp_phase reset inside
            }
        }
        else if( g->state == 3 )      // game over -> initials entry / title
        {
            g->state_timer--;
            if( g->state_timer <= 0 )
            {
                // did the run make the top-5 table? (score > 0 and it
                // beats an existing entry; empty slots read as 0)
                int k = -1;
                if( g->score > 0 )
                {
                    int q;
                    for( q = 0; q < 5; q++ )
                        if( g->score > g->HISCORE[ q ] ) { k = q; break; }
                }
                if( k >= 0 )
                {
                    g->hs_rank = k;
                    g->entry_pos = 0;
                    g->entry_letters[ 0 ] = 'A';
                    g->entry_letters[ 1 ] = 'A';
                    g->entry_letters[ 2 ] = 'A';
                    g->state = 6;   // prompt for initials
                }
                else
                {
                    // init_state resets difficulty to MEDIUM — keep the
                    // player's last choice across games
                    int dsave = g->difficulty;
                    init_state( g );
                    g->difficulty = dsave;
                }
            }
        }
        else if( g->state == 4 )      // title screen
        {
            // title theme loops; (re)start it if it isn't running
            if( get_channel_state( 0 ) != CH_PLAYING ) play_title_music( g );
            if( g->menu_cooldown > 0 ) g->menu_cooldown--;
            if( g->menu_cooldown == 0 )
            {
                if( gamepad_up() > 0 )
                { g->menu_row--; if( g->menu_row < 0 ) g->menu_row = 3;
                  g->menu_cooldown = 10; sfx( g, BLIP ); }
                else if( gamepad_down() > 0 )
                { g->menu_row++; if( g->menu_row > 3 ) g->menu_row = 0;
                  g->menu_cooldown = 10; sfx( g, BLIP ); }
                else if( g->menu_row == 3 && gamepad_left() > 0 )
                { g->difficulty--; if( g->difficulty < 0 ) g->difficulty = 2;
                  g->menu_cooldown = 10; sfx( g, BLIP ); }
                else if( g->menu_row == 3 && gamepad_right() > 0 )
                { g->difficulty++; if( g->difficulty > 2 ) g->difficulty = 0;
                  g->menu_cooldown = 10; sfx( g, BLIP ); }
            }
            if( gamepad_button_a() == 1 || gamepad_button_start() == 1 )
            {
                if( g->menu_row == 0 )
                {
                    play_track( g, level_track( g ) );   // begin the track run
                    start_level( g );
                }
                else if( g->menu_row == 1 ) g->state = 7;   // high scores
                else if( g->menu_row == 2 )
                {
                    g->select_level = 1;
                    g->level = 1;
                    make_shape( g );      // preview data for the screen
                    g->state = 8;         // level select
                    sfx( g, BLIP );
                }
                // row 3 (difficulty): A does nothing — LEFT/RIGHT changes it
            }
        }
        else if( g->state == 6 )      // new high score: initials entry
        {
            if( get_channel_state( 0 ) != CH_PLAYING ) play_title_music( g );
            if( g->menu_cooldown > 0 ) g->menu_cooldown--;
            if( g->menu_cooldown == 0 )
            {
                if( gamepad_up() > 0 )
                {
                    int l = g->entry_letters[ g->entry_pos ] + 1;
                    // 'A'..'Z' then '0'..'9'. All letters are ABOVE '9'
                    // in ASCII, so a bare "l > '9'" catches every letter
                    // and pins the cursor at 'A' — the gap test must be
                    // the RANGE between '9' and 'A' (chars ':' .. '@').
                    if( l > 'Z' ) l = '0';
                    else if( l > '9' && l < 'A' ) l = 'A';
                    g->entry_letters[ g->entry_pos ] = l;
                    g->menu_cooldown = 9; sfx( g, BLIP );
                }
                else if( gamepad_down() > 0 )
                {
                    int l = g->entry_letters[ g->entry_pos ] - 1;
                    // mirror of the UP wrap: below '0' jumps to 'Z',
                    // and the 'A' -> '9' step uses the same gap RANGE
                    // (digits are all BELOW 'A', so a bare "l < 'A'"
                    // would catch every digit and pin at '9').
                    if( l < '0' ) l = 'Z';
                    else if( l > '9' && l < 'A' ) l = '9';
                    g->entry_letters[ g->entry_pos ] = l;
                    g->menu_cooldown = 9; sfx( g, BLIP );
                }
                else if( gamepad_left() > 0 )
                { g->entry_pos--; if( g->entry_pos < 0 ) g->entry_pos = 2;
                  g->menu_cooldown = 9; sfx( g, BLIP ); }
                else if( gamepad_right() > 0 )
                { g->entry_pos++; if( g->entry_pos > 2 ) g->entry_pos = 0;
                  g->menu_cooldown = 9; sfx( g, BLIP ); }
            }
            if( gamepad_button_a() == 1 || gamepad_button_start() == 1 )
            {
                // insert the score at its rank, shifting the rest down
                int k = g->hs_rank;
                int i;
                for( i = 4; i > k; i-- )
                {
                    g->HISCORE[ i ] = g->HISCORE[ i - 1 ];
                    g->HIINIT[ i ][ 0 ] = g->HIINIT[ i - 1 ][ 0 ];
                    g->HIINIT[ i ][ 1 ] = g->HIINIT[ i - 1 ][ 1 ];
                    g->HIINIT[ i ][ 2 ] = g->HIINIT[ i - 1 ][ 2 ];
                }
                g->HISCORE[ k ] = g->score;
                g->HIINIT[ k ][ 0 ] = g->entry_letters[ 0 ];
                g->HIINIT[ k ][ 1 ] = g->entry_letters[ 1 ];
                g->HIINIT[ k ][ 2 ] = g->entry_letters[ 2 ];
                hs_save( g );
                g->state = 7;   // show the updated table
                sfx( g, CLEAR );
            }
        }
        else if( g->state == 7 )      // high scores table
        {
            if( get_channel_state( 0 ) != CH_PLAYING ) play_title_music( g );
            if( gamepad_button_a() == 1 || gamepad_button_b() == 1 ||
                gamepad_button_start() == 1 )
            {
                int dsave = g->difficulty;
                init_state( g );
                g->difficulty = dsave;
            }
        }
        else if( g->state == 8 )      // level select
        {
            if( get_channel_state( 0 ) != CH_PLAYING ) play_title_music( g );
            if( g->menu_cooldown > 0 ) g->menu_cooldown--;
            if( g->menu_cooldown == 0 )
            {
                if( gamepad_right() > 0 )
                {
                    g->select_level++;
                    if( g->select_level > 32 ) g->select_level = 1;
                    g->level = g->select_level;
                    make_shape( g );      // refresh the rotating preview
                    g->menu_cooldown = 10; sfx( g, BLIP );
                }
                else if( gamepad_left() > 0 )
                {
                    g->select_level--;
                    if( g->select_level < 1 ) g->select_level = 32;
                    g->level = g->select_level;
                    make_shape( g );
                    g->menu_cooldown = 10; sfx( g, BLIP );
                }
            }
            if( gamepad_button_a() == 1 || gamepad_button_start() == 1 )
            {
                g->level = g->select_level;
                play_track( g, level_track( g ) );   // begin the track run
                start_level( g );
            }
            else if( gamepad_button_b() == 1 )
            {
                g->level = 1;          // restore the normal PLAY path
                make_shape( g );
                g->state = 4;
            }
        }
        else if( g->state == 5 )      // PAUSE: music keeps playing
        {
            if( g->menu_cooldown > 0 ) g->menu_cooldown--;
            if( g->menu_cooldown == 0 )
            {
                // LEFT/RIGHT: music volume. L/R buttons: SFX volume.
                // UP/DOWN: soundtrack (switches live).
                if( gamepad_right() > 0 )
                {
                    g->music_volume += 0.1;
                    if( g->music_volume > 2.0 ) g->music_volume = 2.0;
                    select_channel( 0 );
                    set_channel_volume( g->music_volume );
                    g->menu_cooldown = 8; sfx( g, BLIP );
                }
                else if( gamepad_left() > 0 )
                {
                    g->music_volume -= 0.1;
                    if( g->music_volume < 0 ) g->music_volume = 0;
                    select_channel( 0 );
                    set_channel_volume( g->music_volume );
                    g->menu_cooldown = 8; sfx( g, BLIP );
                }
                else if( gamepad_button_r() > 0 )
                {
                    g->sfx_volume += 0.1;
                    if( g->sfx_volume > 2.0 ) g->sfx_volume = 2.0;
                    g->menu_cooldown = 8;   // BLIP plays at the new level
                    sfx( g, BLIP );
                }
                else if( gamepad_button_l() > 0 )
                {
                    g->sfx_volume -= 0.1;
                    if( g->sfx_volume < 0 ) g->sfx_volume = 0;
                    g->menu_cooldown = 8;
                    sfx( g, BLIP );
                }
                else if( gamepad_down() > 0 )
                {
                    int t = g->music_index + 1;
                    if( t > 3 ) t = 0;
                    play_track( g, t );
                    g->menu_cooldown = 12; sfx( g, BLIP );
                }
                else if( gamepad_up() > 0 )
                {
                    int t = g->music_index - 1;
                    if( t < 0 ) t = 3;
                    play_track( g, t );
                    g->menu_cooldown = 12; sfx( g, BLIP );
                }
            }
            if( gamepad_button_start() == 1 ) g->state = 0;
        }
        if( g->state != 5 )   // paused: the whole world freezes
        {
            update_particles( g );
            update_shocks( g );
            update_stars( g );
        }
        if( g->state != 3 && g->state != 4 && g->state != 6 &&
            g->state != 7 && g->state != 8 )
            update_music( g );   // track succession (incl. during pause;
                                // menus keep the looping title theme)
        g->frame++;

        // -- render ---------------------------------------------------------
        // insurance: force known GPU state before the clear, so no
        // stale blending mode or multiply color can interfere with it
        set_blending_mode( BLEND_SOLID );
        set_multiply_color( make_color( 255, 255, 255 ) );
        clear_screen( make_color( 2, 2, 8 ) );
        render_starfield( g );
        if( g->state == 4 )
        {
            render_title( g );
            end_frame();
            continue;
        }
        if( g->state == 6 )
        {
            render_entry( g );
            end_frame();
            continue;
        }
        if( g->state == 7 )
        {
            render_scores( g );
            end_frame();
            continue;
        }
        if( g->state == 8 )
        {
            render_levelselect( g );
            end_frame();
            continue;
        }
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
        if( g->state == 5 ) render_pause( g );
        render_message( g );

        end_frame();
    }
}
