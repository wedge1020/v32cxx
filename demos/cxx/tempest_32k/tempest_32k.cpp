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
//      with the character code, then draw_region_at. Base glyph is 10x20 px.
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

#define CX 320
#define CY 168
#define OUT_RX 300
#define OUT_RY 158
#define IN_RX 34
#define IN_RY 18

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

struct G
{
    // math tables
    float SIN_TABLE[ 256 ];

    // web geometry
    float SHAPE[ 16 ];
    float px; float py; float pscale;   // project() outputs

    // entities
    char  GLYPHS[ 6 ];
    Bullet   BULLETS[ 32 ];
    Enemy    ENEMIES[ 24 ];
    Particle PARTICLES[ 220 ];
    float SPIKE[ 16 ];

    // game state
    float player_lane;
    int   lives;
    int   score;
    int   level;
    int   superzaps;
    int   enemies_alive;
    int   spawn_timer;
    int   spawn_interval;
    int   state;               // 0 play, 1 dying, 2 warp-out, 3 game over
    int   state_timer;
    char  message[ 24 ];
    int   message_timer;
    float warp;
    int   launched;
    int   fire_cooldown;
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

float sin_taylor( float x )
{
    while( x > 3.14159265 ) x -= 6.28318531;
    while( x < -3.14159265 ) x += 6.28318531;
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
    }
}

float sin32( G* g, float x )
{
    int i = ( (int)( x * 40.7436611 ) ) & 255;   // 256/(2PI)
    return g->SIN_TABLE[ i ];
}

float cos32( G* g, float x )
{
    return sin32( g, x + 1.57079632 );
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
    draw_region_at( (int)( x - w / 2 ), (int)( y - h / 2 ) );
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
    float rx = lerp( OUT_RX, IN_RX, z ) * g->SHAPE[ ( (int)lane ) % LANES ];
    float ry = lerp( OUT_RY, IN_RY, z );
    g->px = CX + cos32( g, ang ) * rx;
    g->py = CY + sin32( g, ang ) * ry;
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

// ---------------------------------------------------------------------------
//  Input / update
// ---------------------------------------------------------------------------
void kill_player( G* g );

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
            burst( g, g->px, g->py, 14, 3 );
            g->ENEMIES[ i ].alive = 0;
            g->enemies_alive--;
            g->score += 150;
        }
    }
}

void update_player( G* g )
{
    if( gamepad_left() > 0 )  g->player_lane -= 0.09;
    if( gamepad_right() > 0 ) g->player_lane += 0.09;
    if( g->player_lane < 0 ) g->player_lane += LANES;
    if( g->player_lane >= LANES ) g->player_lane -= LANES;

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
    burst( g, g->px, g->py, 60, 5 );
    show_message( g, "OW!" );
}

void level_clear( G* g );

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

        // rim behaviour: lethal if it shares your lane
        if( g->ENEMIES[ i ].z < 0 )
        {
            g->ENEMIES[ i ].z = 0;
            float diff = g->player_lane - g->ENEMIES[ i ].lane;
            if( diff < 0 ) diff = -diff;
            if( diff > LANES / 2 ) diff = LANES - diff;
            if( diff < 0.7 ) { kill_player( g ); }
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
                burst( g, g->px, g->py, 18, 2.4 );

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
                    g->enemies_alive--;
                    g->score += 100;
                    if( g->enemies_alive == 0 && g->spawn_timer > 999 )
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
//  Rendering
// ---------------------------------------------------------------------------
void render_web( G* g )
{
    int i; int r;

    set_blending_mode( BLEND_SOLID );
    set_multiply_color( make_color( 30, 90, 120 ) );
    for( i = 0; i < LANES; i++ )
    {
        project( g, i, 0 );  float x0 = g->px; float y0 = g->py;
        project( g, i, 1 );  draw_dot_line( g, x0, y0, g->px, g->py, 22 );
    }

    // depth rings; during warp-out they RUSH past (warp^2 scroll boost).
    // Budget: ONE dot per lane per ring (7 x 16 = 112 glyphs), NOT a
    // dot-line per segment — the frame budget can't afford that.
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
}

void render_spikes( G* g )
{
    int i;
    set_glow( 2 );
    for( i = 0; i < LANES; i++ )
    {
        if( g->SPIKE[ i ] <= 0 ) continue;
        project( g, i, 1.0 - g->SPIKE[ i ] );  float x0 = g->px; float y0 = g->py;
        project( g, i, 1.0 );                  float x1 = g->px; float y1 = g->py;
        set_multiply_color( make_color( 255, 60, 30 ) );
        draw_dot_line( g, x0, y0, x1, y1, 9 );
        project( g, i, 1.0 - g->SPIKE[ i ] );
        draw_glyph( g, '+', g->px, g->py, 8, 8 );
    }
    set_blending_mode( BLEND_SOLID );
}

void render_player( G* g )
{
    if( g->state == 1 ) return;   // dying: particles only

    project( g, g->player_lane, 0 );
    float x = g->px; float y = g->py; float s = g->pscale;

    // during warp-out the claw dives toward the vanishing point
    float dive = g->warp;
    if( dive > 1 ) dive = 1;
    x = lerp( x, CX, dive );
    y = lerp( y, CY, dive );
    s *= ( 1.0 - dive * 0.9 );

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
        set_multiply_color( make_color( 255, 220 - f, 120 - f / 2 ) );
        float s = 4 + g->PARTICLES[ i ].glyph * 1.5;
        draw_glyph( g, g->GLYPHS[ g->PARTICLES[ i ].glyph ],
                    g->PARTICLES[ i ].x, g->PARTICLES[ i ].y, s, s );
    }
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
    draw_party_text( g, tmp, 320, 18, 16, 0 );

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
    g->enemies_alive = 0;
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
        g->enemies_alive++;
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
    g->lives = 3;
    g->score = 0;
    g->level = 1;
    g->warp = 0;
    g->frame = 0;
    g->rng_state = 12345;
    g->last_region = -1;
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;
    build_tables( g );
}

void main()
{
    select_texture( -1 );           // BIOS font
    select_gamepad( 0 );

    // ALL mutable state lives on the heap: file-scope variables in
    // generated Vircon32 C are read-only (no `global` keyword emitted)
    G* g = new G;
    init_state( g );
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
        else if( g->state == 2 )      // warp-out: fly into the tube
        {
            g->warp += 0.012;
            int li = ( (int)g->player_lane ) % LANES;
            if( g->warp < 0.8 && g->SPIKE[ li ] > 0.3 )
            {
                burst( g, 320, 168, 30, 4 );
                kill_player( g );
                g->warp = 0;
            }
            else
            {
                g->state_timer--;
                if( g->state_timer <= 0 ) { g->level++; g->warp = 0; start_level( g ); }
            }
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
        g->frame++;

        // -- render ---------------------------------------------------------
        // insurance: force known GPU state before the clear, so no
        // stale blending mode or multiply color can interfere with it
        set_blending_mode( BLEND_SOLID );
        set_multiply_color( make_color( 255, 255, 255 ) );
        clear_screen( make_color( 2, 2, 8 ) );
        render_web( g );
        render_spikes( g );
        render_bullets( g );
        render_enemies( g );
        render_player( g );
        render_particles( g );
        render_hud( g );
        render_message( g );

        end_frame();
    }
}
