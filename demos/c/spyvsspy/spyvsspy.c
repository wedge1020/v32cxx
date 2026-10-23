/* =============================================================================
 * Spy vs Spy: Road Wars
 * -----------------------------------------------------------------------------
 * A regular-C game for the v32c++ transpiler (Vircon32 target).
 *
 * PURPOSE (read me first):
 *   1. Regression-test the transpiler's C language coverage: every construct
 *      marked [COV] below is one the docs claim is supported today. If this
 *      file transpiles cleanly AND the generated C looks right, coverage
 *      holds. Anything that fails is a found gap.
 *   2. Stress the Vircon32-rewrite passes: ternaries, comma operators,
 *      structs by pointer, function pointers in BOTH declarator spellings,
 *      arrays in BOTH declarator spellings, and pointer/array arithmetic.
 *
 * GAME FLOW:  TITLE -> PLAY <-> PAUSE, PLAY -> CRASH -> PLAY (next life)
 *             or OVER -> TITLE.  The main loop polls the pad once per frame
 *             into g_pad (with edge detection) and dispatches through the
 *             g_phase_frame table.
 *
 * Companion files:
 *   - textures/spyvsspy_sprites.png : the single texture referenced by the
 *     #texture hint below (must sit at this exact path for the cart XML).
 *   - gap_probes.cpp: the known-gap probe harness, each construct gated
 *                     behind its own -DGAP_xxx macro so failures stay
 *                     isolated.
 *
 * BUILD (the project Makefile's rule):
 *   v32c++ -I ../../../c_api -I inc -o obj/spyvsspy.c spyvsspy.c
 *   then compile -> (v32opt) -> assemble -> png2vircon -> packrom.
 *
 * STYLE NOTES:
 *   - Plain C89-flavoured C. main is declared int main() on purpose: the
 *     transpiler must force void main(void) and strip the trailing return.
 *   - No double quotes inside comments (keeps simple text tools honest).
 * ========================================================================== */

/* ----------------------------------------------------------------------------
 * Cart hints (recognized by v32c++'s lexer; drive the generated XML)
 * -------------------------------------------------------------------------- */
#texture TEX_SPRITES "textures/spyvsspy_sprites.png"
#title "Spy vs Spy: Road Wars"
#version 1.1

/* ----------------------------------------------------------------------------
 * Pass-through SDK headers (carried into the generated C verbatim)
 * -------------------------------------------------------------------------- */
#include "video.h"
#include "input.h"
#include "time.h"
#include "misc.h"

/* ----------------------------------------------------------------------------
 * [COV] #ifdef / #else / #endif evaluation against a predefined macro
 * -------------------------------------------------------------------------- */
#ifdef __V32CXX__
    #define BUILD_LABEL "V32C++ REGRESSION CART"
#else
    #define BUILD_LABEL "UNKNOWN BUILD"
#endif
/* The indentation above is deliberate: indented directives were gap #1 and
 * this block is its regression test. */

/* ----------------------------------------------------------------------------
 * Tunables. Function-like macros with parenthesized bodies.
 *
 * Road layout (screen is 640x360, tiles are 32x32, drawn on a grid shifted
 * left by half a tile so the road is exactly centred):
 *     shoulder 112..144 | edge 144..176 | asphalt 176..464 | edge 464..496
 *     | shoulder 496..528
 * The edge tiles carry their painted line on the INNER side, so the drivable
 * area is exactly ROAD_LEFT..ROAD_RIGHT: three lanes of 96 px.
 * -------------------------------------------------------------------------- */
#define ROAD_LEFT     176
#define ROAD_RIGHT    464
#define LANE_COUNT    3
#define LANE_WIDTH    (( ROAD_RIGHT - ROAD_LEFT ) / LANE_COUNT)   /* 96 */
#define LANE_X( l )   ( ROAD_LEFT + LANE_WIDTH/2 + (l) * LANE_WIDTH )
#define LANE_DIV( k ) ( ROAD_LEFT + (k) * LANE_WIDTH )

#define TILE          32
#define MAX_ACTORS    24
#define PATTERN_COUNT 8
#define FUEL_MAX      100
#define START_LIVES   3
#define PLAYER_Y      282
#define MAX_SPEED     5
#define MAX_LEVEL     8
#define LEVEL_LENGTH  6000      /* distance units per difficulty level */

/* actor flag bits: bitwise coverage runs through the whole game logic */
#define F_WHITE   1     /* reserved: white-spy variant sprite        */
#define F_OIL     2     /* sliding on oil                            */
#define F_SPIKES  4     /* flat tires, speed halved                  */
#define F_FUEL    8     /* reserved: this obstacle is a fuel can     */
#define F_ALL     15

/* region ids inside texture TEX_SPRITES (must match sprites.png layout) */
#define RG_PLAYER_BLACK  0    /* 24x40 cars, hotspot centred   */
#define RG_PLAYER_WHITE  1
#define RG_RIVAL_BLACK   2
#define RG_RIVAL_WHITE   3
#define RG_TRAFFIC_GRAY  4
#define RG_TRAFFIC_BROWN 5
#define RG_TRUCK         6
#define RG_ASPHALT       7    /* 32x32 tiles, hotspot top-left */
#define RG_ASPHALT_DASH  8
#define RG_ROAD_EDGE_L   9
#define RG_ROAD_EDGE_R   10
#define RG_GRASS         11
#define RG_GRASS_BUSH    12
#define RG_SHOULDER      13
#define RG_OIL           14    /* 32x24, centred */
#define RG_SPIKES        15    /* 32x12, centred */
#define RG_BARREL        16    /* 16x16, centred */
#define RG_CRATE         17
#define RG_FUELCAN       18
#define RG_MUZZLE        19    /* 8x8, centred   */
#define RG_EXPLO_A       20    /* 16x16, centred */
#define RG_EXPLO_B       21
#define RG_EXPLO_C       22
#define RG_SMOKE         23
#define RG_SKID          24    /* 32x16, centred */
#define RG_BULLET_PLY    25    /* 4x10, centred  */
#define RG_BULLET_RIV    26
#define RG_LAMPPOST      27    /* 16x32, hotspot base-centre */
#define RG_SIGN          28    /* 24x24 */
#define RG_TREE          29

/* ----------------------------------------------------------------------------
 * [COV] enum: explicit values AND auto-increment in one list
 * -------------------------------------------------------------------------- */
enum ActorKind
{
    KIND_NONE     = 0,
    KIND_PLAYER   = 1,
    KIND_TRAFFIC  = 2,
    KIND_RIVAL    = 3,
    KIND_BULLET   = 4,
    KIND_OBSTACLE = 5,
    KIND_EFFECT   = 6,
    KIND_PROP     = 7,
    KIND_COUNT          /* auto-increment: 8 */
};

enum GamePhase
{
    PH_TITLE = 0,
    PH_PLAY  = 1,
    PH_CRASH = 2,
    PH_OVER  = 3,
    PH_PAUSE = 4,
    PH_COUNT       /* 5 */
};

/* ----------------------------------------------------------------------------
 * [COV] plain C structs (one-word members; only ever passed by pointer)
 * -------------------------------------------------------------------------- */
struct Actor
{
    int kind;
    int x;
    int y;
    int vx;
    int vy;
    int w;          /* hitbox, not sprite size */
    int h;
    int region;
    int life;
    int flags;
    bool alive;
};

struct GameState
{
    int phase;
    int score;
    int lives;
    int fuel;
    int speed;
    int pace;           /* world scroll in px/frame, computed once per frame */
    int level;
    int distance;
    int frame;
    int invuln;
    int crash_timer;
    int spawn_timer;
    int flat_tire;
    int fire_cooldown;
    int shift_timer;    /* gas/brake auto-repeat */
    int fuel_timer;
    int banner_timer;   /* LEVEL UP flash */
    int spy_white;
    float world_y;
};

/* pad state, polled once per frame with edge detection */
struct PadState
{
    int dx;             /* -1 left, +1 right */
    int dy;             /* -1 up (gas), +1 down (brake) */
    int fire;
    int start_edge;     /* START went down this frame */
    int left_edge;
    int right_edge;
    int start_held;
    int left_held;
    int right_held;
};

/* [COV] the canonical C tag typedefs, written AFTER the definitions */
typedef struct Actor Actor;
typedef struct GameState GameState;
typedef struct PadState PadState;

/* [COV] function-pointer typedefs in the STANDARD-C declarator spelling,
 * one with a NAMED parameter; the generated C comes out in Vircon32's
 * reversed form regardless. */
typedef void (*ActorFn)( Actor* a );
typedef void (*FrameFn)();

/* ----------------------------------------------------------------------------
 * Globals.
 * [COV] file-scope variables; braced initializer lists; binary literals;
 *       a string literal as an int-array initializer; the Vircon32-native
 *       length-before-name array declarator, accepted interchangeably.
 * -------------------------------------------------------------------------- */
Actor     g_actors[ MAX_ACTORS ];
Actor*    g_player;                      /* assigned in start_round, 0 on title */
GameState g_game;
PadState  g_pad;
int       g_hiscore;                     /* survives restarts, not power-off */
int       g_new_hi;

ActorFn g_kind_update[ KIND_COUNT ];     /* [COV] enum constant as array dim */
FrameFn g_phase_frame[ PH_COUNT ];

/* back-to-front draw order (the player sits between rivals and bullets) */
int g_layer_order[ 7 ] = { KIND_PROP, KIND_OBSTACLE, KIND_TRAFFIC, KIND_RIVAL,
                           KIND_PLAYER, KIND_BULLET, KIND_EFFECT };

/* standard declarator + braced init list of binary literals */
int g_pattern_bits[ PATTERN_COUNT ] = { 0b0001, 0b0010, 0b0100, 0b1000,
                                        0b0011, 0b0110, 0b1100, 0b0101 };

/* Vircon32-native declarator spelling (as the SDK's own headers write it).
 * Extra downward speed (px/frame, minus 2) of the loose obstacles in a wave. */
int[ PATTERN_COUNT ] g_pattern_speed = { 2, 2, 2, 2, 3, 3, 4, 4 };

/* string literal expanding to an int list + terminator */
int g_msg_over[ 16 ] = "GAME OVER";

/* ----------------------------------------------------------------------------
 * [COV] prototypes (the prototype/definition dedupe path)
 * -------------------------------------------------------------------------- */
void update_player( Actor* a );
void update_traffic( Actor* a );
void update_rival( Actor* a );
void update_bullet( Actor* a );
void update_effect( Actor* a );
void update_prop( Actor* a );
void update_obstacle( Actor* a );
void update_none( Actor* a );
int  g_player_x();
void crash_player();
void spawn_effect( int x, int y, int big );
void start_round();
void enter_title();
void title_frame();
void play_frame();
void crash_frame();
void over_frame();
void pause_frame();

/* =============================================================================
 * small helpers
 * ========================================================================== */

/* [COV] inline (accepted, ignored) + const-qualified parameters + a ternary
 * CHAIN in a return expression */
inline int clampi( const int v, const int lo, const int hi )
{
    return v < lo ? lo : ( v > hi ? hi : v );
}

/* [COV] ternary in a return expression */
int max2( int a, int b )
{
    return a > b ? a : b;
}

/* Scale the three colour channels by 1/2^shift and keep alpha. Works on the
 * packed word with plain shifts and masks (every int is one 32-bit word on
 * Vircon32, so a union cannot split it into bytes). */
int dim_color( int color, int shift )
{
    int c0 = color & 255;
    int c1 = ( color >> 8 ) & 255;
    int c2 = ( color >> 16 ) & 255;
    int c3 = ( color >> 24 ) & 255;
    return ( c3 << 24 ) | ( ( c2 >> shift ) << 16 )
         | ( ( c1 >> shift ) << 8 ) | ( c0 >> shift );
}

/* [COV] do/while + int division/modulo; writes a 0-terminated int string */
void format_int( int value, int* out )
{
    int digits[ 12 ];
    int n = 0;
    int i;
    if( value < 0 )
        value = -value;
    if( value == 0 )
    {
        out[ 0 ] = '0';
        out[ 1 ] = 0;
        return;
    }
    do
    {
        digits[ n ] = '0' + value % 10;    /* [COV] char literal */
        ++n;
        value /= 10;
    } while( value > 0 );
    for( i = 0; i < n; ++i )
        out[ i ] = digits[ n - 1 - i ];
    out[ n ] = 0;
}

/* BIOS font glyphs are 10 px wide, so centring is just arithmetic */
int text_len( int* s )
{
    int n = 0;
    while( s[ n ] != 0 )
        ++n;
    return n;
}

void print_center( int y, int* text )
{
    print_at( 320 - text_len( text ) * 5, y, text );
}

int pressed( int v )
{
    if( v > 0 )
        return 1;
    return 0;
}

int edge( int now, int held )
{
    if( now != 0 && held == 0 )
        return 1;
    return 0;
}

/* One poll per frame. Steering comes from the analog-style direction pair
 * ([COV] pointer out-params into an SDK function); gas and brake come from
 * the explicit up/down buttons so their sign never depends on the axis
 * convention. Left/right/START also get edge flags for menu use. */
void poll_input()
{
    int dx = 0;
    int dy = 0;
    int left_now = 0;
    int right_now = 0;
    int start_now = pressed( gamepad_button_start() );

    gamepad_direction( &dx, &dy );
    if( dx < 0 )
        left_now = 1;
    if( dx > 0 )
        right_now = 1;

    g_pad.dx = right_now - left_now;
    g_pad.dy = pressed( gamepad_down() ) - pressed( gamepad_up() );
    g_pad.fire = pressed( gamepad_button_a() );

    g_pad.start_edge = edge( start_now, g_pad.start_held );
    g_pad.left_edge  = edge( left_now, g_pad.left_held );
    g_pad.right_edge = edge( right_now, g_pad.right_held );
    g_pad.start_held = start_now;
    g_pad.left_held  = left_now;
    g_pad.right_held = right_now;
}

/* one BIOS-font block cell, tinted; restores texture + neutral colour */
void draw_bios_block( int x, int y, int region_id, int color )
{
    set_multiply_color( color );
    select_texture( -1 );
    select_region( region_id );
    draw_region_at( x, y );
    select_texture( TEX_SPRITES );
    set_multiply_color( color_white );
}

/* scale <= 1 = plain draw; scale >= 2 = centred zoom (title preview) */
void draw_sprite_scaled( int region_id, int x, int y, int scale )
{
    select_texture( TEX_SPRITES );
    select_region( region_id );
    if( scale > 1 )
    {
        set_drawing_scale( (float)scale, (float)scale );  /* [COV] int->float cast */
        draw_region_zoomed_at( x, y );
        set_drawing_scale( 1.0f, 1.0f );    /* [COV] float literal suffix */
    }
    else
    {
        draw_region_at( x, y );
    }
}

/* =============================================================================
 * region setup: every sprite in sprites.png gets its region defined
 * ========================================================================== */
void setup_regions()
{
    select_texture( TEX_SPRITES );

    /* cars: 24x40, hotspot centred via the 6-argument define_region */
    select_region( RG_PLAYER_BLACK );  define_region(   0, 0,  23, 39, 12, 20 );
    select_region( RG_PLAYER_WHITE );  define_region(  32, 0,  55, 39, 12, 20 );
    select_region( RG_RIVAL_BLACK );   define_region(  64, 0,  87, 39, 12, 20 );
    select_region( RG_RIVAL_WHITE );   define_region(  96, 0, 119, 39, 12, 20 );
    select_region( RG_TRAFFIC_GRAY );  define_region( 128, 0, 151, 39, 12, 20 );
    select_region( RG_TRAFFIC_BROWN ); define_region( 160, 0, 183, 39, 12, 20 );
    select_region( RG_TRUCK );         define_region( 192, 0, 215, 39, 12, 20 );

    /* tiles: 32x32, hotspot top-left (the 4-argument form) */
    select_region( RG_ASPHALT );      define_region_topleft(   0, 48,  31, 79 );
    select_region( RG_ASPHALT_DASH ); define_region_topleft(  32, 48,  63, 79 );
    select_region( RG_ROAD_EDGE_L );  define_region_topleft(  64, 48,  95, 79 );
    select_region( RG_ROAD_EDGE_R );  define_region_topleft(  96, 48, 127, 79 );
    select_region( RG_GRASS );        define_region_topleft( 128, 48, 159, 79 );
    select_region( RG_GRASS_BUSH );   define_region_topleft( 160, 48, 191, 79 );
    select_region( RG_SHOULDER );     define_region_topleft( 192, 48, 223, 79 );

    /* obstacles, centred */
    select_region( RG_OIL );      define_region(   0,  88,  31, 111, 16, 12 );
    select_region( RG_SPIKES );   define_region(  40,  88,  71,  99, 16,  6 );
    select_region( RG_BARREL );   define_region(  80,  88,  95, 103,  8,  8 );
    select_region( RG_CRATE );    define_region( 100,  88, 115, 103,  8,  8 );
    select_region( RG_FUELCAN );  define_region( 120,  88, 135, 103,  8,  8 );

    /* effects, centred */
    select_region( RG_MUZZLE );   define_region(   0, 112,   7, 119,  4,  4 );
    select_region( RG_EXPLO_A );  define_region(  12, 112,  27, 127,  8,  8 );
    select_region( RG_EXPLO_B );  define_region(  32, 112,  47, 127,  8,  8 );
    select_region( RG_EXPLO_C );  define_region(  52, 112,  67, 127,  8,  8 );
    select_region( RG_SMOKE );    define_region(  72, 112,  87, 127,  8,  8 );
    select_region( RG_SKID );     define_region( 112, 112, 143, 127, 16,  8 );

    /* bullets, centred */
    select_region( RG_BULLET_PLY ); define_region( 0, 140,   3, 149, 2, 5 );
    select_region( RG_BULLET_RIV ); define_region( 8, 140,  11, 149, 2, 5 );

    /* roadside props: hotspot at the base so they plant on the ground */
    select_region( RG_LAMPPOST ); define_region(  0, 152,  15, 183,  8, 32 );
    select_region( RG_SIGN );    define_region( 20, 152,  43, 175, 12, 24 );
    select_region( RG_TREE );    define_region( 48, 152,  71, 175, 12, 24 );
}

/* =============================================================================
 * actor pool
 * ========================================================================== */

void clear_actors()
{
    int i;
    /* [COV] sizeof an array expression + memset from a pass-through header */
    memset( g_actors, 0, sizeof g_actors );
    for( i = 0; i < MAX_ACTORS; ++i )
        g_actors[ i ].alive = false;
}

/* returns the slot index, or -1 when the pool is full */
int spawn_actor( int kind, int x, int y, int w, int h, int region, int flags )
{
    /* [COV] pointer iteration: array decay, pointer arithmetic, ++ on a
     * pointer, != on pointers */
    Actor* it  = g_actors;
    Actor* end = g_actors + MAX_ACTORS;
    while( it != end )
    {
        if( it->alive == false )
        {
            it->kind   = kind;
            it->x      = x;
            it->y      = y;
            it->vx     = 0;
            it->vy     = 0;
            it->w      = w;
            it->h      = h;
            it->region = region;
            it->life   = 0;
            it->flags  = flags;
            it->alive  = true;
            return (int)( it - g_actors );   /* [COV] pointer difference + cast */
        }
        ++it;
    }
    return -1;
}

/* [COV] sizeof applied to a pointer-dereference expression (no parens
 * needed: sizeof *a) */
void kill_actor( Actor* a )
{
    memset( a, 0, sizeof *a );
    a->alive = false;
}

/* a short-lived visual with a frame countdown */
int spawn_fx( int x, int y, int region, int life )
{
    int slot = spawn_actor( KIND_EFFECT, x, y, 16, 16, region, 0 );
    if( slot >= 0 )
        g_actors[ slot ].life = life;
    return slot;
}

void spawn_effect( int x, int y, int big )
{
    int i;
    int spread = 10;    /* [COV] ternary directly initializing a local */
    int step = big ? 6 : 3;
    if( big )
        spread = 18;
    for( i = 0; i < 3; ++i )
        spawn_fx( x + ( i - 1 ) * spread, y + ( i - 1 ) * step,
                  RG_EXPLO_C, 24 - i * 6 );
    spawn_fx( x, y - 10, RG_SMOKE, 30 );
}

/* [COV] const-qualified pointer parameters, bool return */
bool aabb_hit( const Actor* a, const Actor* b )
{
    int dx = a->x - b->x;
    int dy = a->y - b->y;
    if( dx < 0 ) dx = -dx;
    if( dy < 0 ) dy = -dy;
    return dx < ( a->w + b->w ) / 2 && dy < ( a->h + b->h ) / 2;
}

int count_kind( int kind )
{
    int n = 0;
    int i;
    for( i = 0; i < MAX_ACTORS; ++i )
        if( g_actors[ i ].alive && g_actors[ i ].kind == kind )
            ++n;
    return n;
}

/* is there room to spawn a vehicle or obstacle at (x, y)? */
bool lane_clear( int x, int y )
{
    int i;
    for( i = 0; i < MAX_ACTORS; ++i )
    {
        Actor* o = &g_actors[ i ];
        if( o->alive && ( o->kind == KIND_TRAFFIC || o->kind == KIND_RIVAL ||
                          o->kind == KIND_OBSTACLE ) )
        {
            int dx = o->x - x;
            int dy = o->y - y;
            if( dx < 0 ) dx = -dx;
            if( dy < 0 ) dy = -dy;
            if( dx < 40 && dy < 70 )
                return false;
        }
    }
    return true;
}

/* =============================================================================
 * per-kind updates (dispatched through the g_kind_update table).
 * Everything that scrolls with the road uses g_game.pace, which play_frame
 * computes once per frame (flat tires included), so the world stays in sync.
 * ========================================================================== */
void update_none( Actor* a )
{
    /* KIND_NONE only: dead pool slots never tick, but the dispatch table
     * needs a function for every kind. */
    a->flags = a->flags & F_ALL;
}

void update_player( Actor* a )
{
    int steer = 4;

    /* flat tires from the spike strip: mushy steering until they wear off */
    if( g_game.flat_tire > 0 )
    {
        g_game.flat_tire -= 1;
        steer = 2;
        if( g_game.flat_tire == 0 )
            a->flags &= F_ALL ^ F_SPIKES;
    }

    /* gas / brake: up = faster, down = slower, auto-repeating */
    if( g_pad.dy == 0 )
        g_game.shift_timer = 0;
    else if( g_game.shift_timer > 0 )
        g_game.shift_timer -= 1;
    else
    {
        g_game.speed = clampi( g_game.speed - g_pad.dy, 0, MAX_SPEED );
        g_game.shift_timer = 8;
    }

    /* oil slick: steering becomes a slide; drift carries the last vx */
    if( ( a->flags & F_OIL ) != 0 )   /* [COV] & with explicit parens --
                                         C's a & b == c binds == tighter */
    {
        if( a->life > 0 )
        {
            a->life -= 1;
            a->x += a->vx * 3;
            if( ( g_game.frame & 3 ) == 0 )
                spawn_fx( a->x, a->y + 26, RG_SKID, 10 );
        }
        else
        {
            a->flags &= F_ALL ^ F_OIL;    /* clear the oil bit, no ~ needed */
        }
    }
    else
    {
        /* [COV] ternary directly assigned to a bare identifier:
         * pressing up gives a little extra steering bite */
        steer = ( g_pad.dy < 0 ) ? steer + 2 : steer;
        a->vx = g_pad.dx * steer;
        a->x += a->vx;
    }

    /* keep the whole car on asphalt */
    a->x = clampi( a->x, ROAD_LEFT + 12, ROAD_RIGHT - 12 );

    /* fuel burn: one unit every 30 - 3*speed frames (27 s .. 17 s per tank
     * at 60 fps, plus fuel cans) */
    g_game.fuel_timer += 1;
    if( g_game.fuel_timer >= 30 - g_game.speed * 3 )
    {
        g_game.fuel_timer = 0;
        g_game.fuel -= 1;
        if( g_game.fuel <= 0 )
        {
            g_game.fuel = 0;
            crash_player();
            return;
        }
    }

    /* distance and score tick up with pace */
    g_game.distance += g_game.pace;
    if( ( g_game.frame & 7 ) == 0 )
        g_game.score += 1 + g_game.speed / 2;

    /* shooting: hold button A, at most every 8 frames */
    if( g_game.fire_cooldown > 0 )
        g_game.fire_cooldown -= 1;
    if( g_pad.fire != 0 && g_game.fire_cooldown == 0 )
    {
        int slot = spawn_actor( KIND_BULLET, a->x, a->y - 24, 6, 12,
                                RG_BULLET_PLY, 0 );
        if( slot >= 0 )
        {
            g_actors[ slot ].vy = -( g_game.pace + 9 );      /* [COV] unary minus */
            spawn_fx( a->x, a->y - 30, RG_MUZZLE, 2 );
            g_game.fire_cooldown = 8;
        }
    }
}

void update_traffic( Actor* a )
{
    /* traffic moves slower than the player, so it drifts down the screen */
    a->y += g_game.pace - 2;
    if( a->y > screen_height + 48 )
        a->alive = false;
}

void update_rival( Actor* a )
{
    int target = g_player_x();

    /* steer toward the player's lane */
    if( a->x < target - 10 )
        a->vx = 2;
    else if( a->x > target + 10 )
        a->vx = -2;
    else
        a->vx = 0;
    a->x = clampi( a->x + a->vx, ROAD_LEFT + 12, ROAD_RIGHT - 12 );

    /* catch up while far above, then pull alongside: the rival scrolls
     * with the road, and while it has ground to make up it closes FASTER
     * than the scroll, which on screen means moving DOWN toward the player */
    if( a->y < PLAYER_Y - 130 )
        a->y += g_game.pace + 2;
    else
        a->y += g_game.pace - 2;

    /* the rival shoots back: same KIND_BULLET, different sprite region --
     * update_bullet tells the two apart by region */
    if( a->y > 0 && a->y < PLAYER_Y - 20 &&
        ( g_game.frame & 31 ) == 0 && ( rand() & 1 ) == 0 )
    {
        int slot = spawn_actor( KIND_BULLET, a->x, a->y + 26, 6, 12,
                                RG_BULLET_RIV, 0 );
        if( slot >= 0 )
            g_actors[ slot ].vy = g_game.pace + 7;    /* faster than the road */
    }

    if( a->y > screen_height + 48 )
        a->alive = false;
}

void update_bullet( Actor* a )
{
    Actor* it  = g_actors;
    Actor* end = g_actors + MAX_ACTORS;
    bool from_rival = ( a->region == RG_BULLET_RIV );
    a->y += a->vy;
    if( a->y < -20 || a->y > screen_height + 20 )
    {
        a->alive = false;
        return;
    }
    if( from_rival )
    {
        /* a rival shot only threatens the player: it flies over traffic
         * and obstacles, and it dies harmlessly while you are invulnerable */
        if( g_player != 0 && aabb_hit( a, g_player ) )
        {
            a->alive = false;
            if( g_game.invuln == 0 )
                crash_player();
        }
        return;
    }
    while( it != end )
    {
        bool solid = true;
        if( it->alive && it != a &&
            ( it->kind == KIND_RIVAL || it->kind == KIND_TRAFFIC ||
              it->kind == KIND_OBSTACLE ) )
        {
            if( aabb_hit( a, it ) )
            {
                /* bullets fly over oil, spikes and fuel cans */
                if( it->kind == KIND_OBSTACLE &&
                    ( it->region == RG_OIL || it->region == RG_SPIKES ||
                      it->region == RG_FUELCAN ) )
                    solid = false;
                if( solid )
                {
                    /* [COV] switch with a stacked fall-through label pair,
                     * a default, and a break with no enclosing loop */
                    switch( it->kind )
                    {
                        case KIND_RIVAL:
                        case KIND_TRAFFIC:
                        {
                            int bounty = 40;
                            bounty = it->kind == KIND_RIVAL ? 250 : 40;
                            g_game.score += bounty;
                            break;
                        }
                        default:
                            g_game.score += 25;
                            break;
                    }
                    spawn_effect( it->x, it->y, it->kind == KIND_RIVAL );
                    kill_actor( it );
                    a->alive = false;
                    return;
                }
            }
        }
        ++it;
    }
}

void update_effect( Actor* a )
{
    int stage;
    if( a->life > 0 )
        a->life -= 1;
    else
    {
        a->alive = false;
        return;
    }
    if( a->region != RG_MUZZLE )
        a->y += g_game.pace;             /* everything but the flash rides the road */
    if( a->y > screen_height + 48 )
    {
        a->alive = false;
        return;
    }
    if( a->region == RG_SMOKE || a->region == RG_SKID ||
        a->region == RG_MUZZLE )
        return;                          /* no frame cycling */
    stage = a->life / 8;
    /* [COV] switch with stacked labels (fall-through) + default */
    switch( stage )
    {
        case 2:
        case 3:
            a->region = RG_EXPLO_C;
            break;
        case 1:
            a->region = RG_EXPLO_B;
            break;
        default:
            a->region = RG_EXPLO_A;
            break;
    }
}

void update_prop( Actor* a )
{
    a->y += g_game.pace;
    if( a->y > screen_height + 48 )
        a->alive = false;
}

/* hazards and solid obstacles scroll with the road; loose ones (barrels,
 * crates, cans) also carry a little extra vy from their wave pattern */
void update_obstacle( Actor* a )
{
    a->y += g_game.pace + a->vy;
    if( a->y > screen_height + 48 )
        a->alive = false;
}

/* one-word int return (Vircon32 ABI); a tiny accessor the rival AI uses */
int g_player_x()
{
    if( g_player != 0 )                  /* 0-vs-pointer compare is rewritten */
        return g_player->x;
    return LANE_X( 1 );
}

/* =============================================================================
 * collisions against the player + crash handling
 * ========================================================================== */
void crash_player()
{
    if( g_game.phase == PH_CRASH )       /* only one crash per life */
        return;
    g_game.phase = PH_CRASH;
    g_game.crash_timer = 46;
    g_game.pace = 0;                     /* the world freezes */
    g_game.lives -= 1;
    if( g_player != 0 )
        spawn_effect( g_player->x, g_player->y, true );
}

/* after a crash with lives left: clear the danger zone and reset the car */
void respawn_player()
{
    int i;
    for( i = 0; i < MAX_ACTORS; ++i )
    {
        Actor* o = &g_actors[ i ];
        if( o->alive && o->y > PLAYER_Y - 150 &&
            ( o->kind == KIND_TRAFFIC || o->kind == KIND_RIVAL ||
              o->kind == KIND_OBSTACLE || o->kind == KIND_BULLET ) )
            kill_actor( o );
    }
    if( g_player != 0 )
    {
        g_player->x = LANE_X( 1 );
        g_player->vx = 0;
        g_player->life = 0;
        g_player->flags = 0;
    }
    g_game.speed = 1;
    g_game.flat_tire = 0;
    g_game.fuel = FUEL_MAX;
    g_game.fuel_timer = 0;
    g_game.invuln = 90;
}

void collide_player()
{
    int i;
    if( g_player == 0 )
        return;
    for( i = 1; i < MAX_ACTORS; ++i )
    {
        Actor* other = &g_actors[ i ];
        if( other->alive == false )
            continue;                   /* [COV] continue in a for loop */
        if( aabb_hit( g_player, other ) == false )
            continue;

        if( other->kind == KIND_RIVAL || other->kind == KIND_TRAFFIC )
        {
            if( g_game.invuln > 0 )
                continue;
            crash_player();
            return;
        }
        if( other->kind == KIND_OBSTACLE )
        {
            /* [COV] switch on an expression; stacked cases fall through */
            switch( other->region )
            {
                case RG_OIL:
                case RG_SPIKES:
                    /* drive-over hazards: status effects, never fatal */
                    if( other->region == RG_OIL )
                    {
                        g_player->flags |= F_OIL;
                        g_player->life = 30;         /* slide timer */
                    }
                    else
                    {
                        g_player->flags |= F_SPIKES;
                        g_game.flat_tire = 90;
                    }
                    break;
                case RG_FUELCAN:
                    g_game.fuel = clampi( g_game.fuel + 35, 0, FUEL_MAX );
                    g_game.score += 50;
                    kill_actor( other );
                    break;
                default:
                    /* barrel / crate: solid, so it hurts */
                    if( g_game.invuln > 0 )
                        continue;
                    spawn_effect( other->x, other->y, false );
                    kill_actor( other );
                    crash_player();
                    return;
            }
        }
    }
}

/* =============================================================================
 * spawning director (gets busier with g_game.level)
 * ========================================================================== */
void spawn_obstacle( int lane, int pick )
{
    int what = rand() & 3;
    int region = RG_BARREL;
    int w = 16;                 /* hitbox matches the sprite */
    int h = 16;
    int slot;
    region = what == 0 ? RG_OIL : ( what == 1 ? RG_SPIKES : RG_BARREL );
    if( what == 2 )
        region = RG_CRATE;
    if( rand() % 12 == 0 )                  /* fuel cans stay rare */
        region = RG_FUELCAN;
    if( region == RG_OIL )
    {
        w = 32;
        h = 24;
    }
    else if( region == RG_SPIKES )
    {
        w = 32;
        h = 12;
    }
    slot = spawn_actor( KIND_OBSTACLE, LANE_X( lane ), -48 - lane * 24,
                        w, h, region, 0 );
    /* painted-on hazards ride the road; loose cargo creeps toward you */
    if( slot >= 0 && region != RG_OIL && region != RG_SPIKES )
        g_actors[ slot ].vy = g_pattern_speed[ pick ] - 2;
}

void spawn_wave()
{
    int pick = rand() & ( PATTERN_COUNT - 1 );    /* [COV] rand + & mask */
    int bits = g_pattern_bits[ pick ];
    int lane;
    int rival_odds = 6 - g_game.level / 2;        /* 1 in 6 .. 1 in 2 */
    int rival_region = RG_RIVAL_BLACK;

    /* rivals drive the spy you did NOT pick */
    rival_region = g_game.spy_white ? RG_RIVAL_BLACK : RG_RIVAL_WHITE;

    if( rand() % rival_odds == 0 && count_kind( KIND_RIVAL ) < 1 + g_game.level / 3 )
    {
        int rx = LANE_X( rand() % LANE_COUNT );
        if( lane_clear( rx, -48 ) )
            spawn_actor( KIND_RIVAL, rx, -48, 20, 34, rival_region, 0 );
        return;
    }

    for( lane = 0; lane < LANE_COUNT; ++lane )
        if( ( bits & ( 1 << lane ) ) != 0 )       /* [COV] variable shift */
            spawn_obstacle( lane, pick );

    /* neutral traffic shares the road */
    if( ( rand() & 1 ) != 0 )
    {
        int traffic_region = RG_TRAFFIC_GRAY;     /* chained ternary to a bare
                                                     identifier */
        int tx = LANE_X( rand() % LANE_COUNT );
        traffic_region = ( rand() & 7 ) == 0 ? RG_TRUCK
                         : ( ( rand() & 1 ) ? RG_TRAFFIC_BROWN
                                            : RG_TRAFFIC_GRAY );
        if( lane_clear( tx, -56 ) )
            spawn_actor( KIND_TRAFFIC, tx, -56, 20, 34, traffic_region, 0 );
    }

    /* roadside dressing on the grass */
    if( ( rand() & 7 ) == 0 )
    {
        int prop_region = RG_LAMPPOST;
        int prop_x = 64;
        prop_region = ( rand() & 1 ) ? RG_SIGN
                       : ( ( rand() & 1 ) ? RG_TREE : RG_LAMPPOST );
        if( rand() & 1 )
            prop_x = 20 + rand() % 80;            /* left grass  */
        else
            prop_x = 530 + rand() % 80;           /* right grass */
        spawn_actor( KIND_PROP, prop_x, -48, 24, 32, prop_region, 0 );
    }
}

/* =============================================================================
 * drawing
 * ========================================================================== */
void draw_road()
{
    /* [COV] register (accepted, ignored); float member narrowed by a cast */
    register int row;
    int col;
    int world_row = ( (int)g_game.world_y ) / TILE;
    int row_off = ( (int)g_game.world_y ) % TILE;
    int base_y = row_off - TILE;

    select_texture( TEX_SPRITES );
    for( row = 0; row <= 12; ++row )
    {
        int y = base_y + row * TILE;
        for( col = 0; col <= 20; ++col )
        {
            int region;
            /* [COV] switch with a fall-through label stack + default */
            switch( col )
            {
                case 4:
                case 16:
                    region = RG_SHOULDER;
                    break;
                case 5:
                    region = RG_ROAD_EDGE_L;
                    break;
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                    region = RG_ASPHALT;
                    break;
                case 15:
                    region = RG_ROAD_EDGE_R;
                    break;
                default:
                    region = RG_GRASS;
                    break;
            }
            /* bushes are keyed to the WORLD row, so they scroll with the grass */
            if( region == RG_GRASS )
            {
                int weave = ( col * 7 + ( world_row - row ) * 13 ) % 11;
                if( weave == 0 )
                    region = RG_GRASS_BUSH;
            }
            select_region( region );
            draw_region_at( col * TILE - TILE / 2, y );
        }
        /* lane dividers: the dash tile is centred, so it straddles the line */
        select_region( RG_ASPHALT_DASH );
        draw_region_at( LANE_DIV( 1 ) - TILE / 2, y );
        draw_region_at( LANE_DIV( 2 ) - TILE / 2, y );
    }
}

void draw_actor( Actor* a )
{
    if( a->kind == KIND_PLAYER )
    {
        if( g_game.phase == PH_CRASH )
            return;                    /* the wreck is the explosion */
        if( g_game.invuln > 0 && ( g_game.frame & 2 ) != 0 )
            return;                    /* blink while invulnerable */
    }
    select_texture( TEX_SPRITES );
    select_region( a->region );
    draw_region_at( a->x, a->y );
}

/* back to front, one pass per kind */
void draw_actors()
{
    int layer;
    int i;
    for( layer = 0; layer < 7; ++layer )
        for( i = 0; i < MAX_ACTORS; ++i )
            if( g_actors[ i ].alive && g_actors[ i ].kind == g_layer_order[ layer ] )
                draw_actor( &g_actors[ i ] );
}

void draw_hud()
{
    int i, lx;         /* [COV] multiple plain declarators */
    int buf[ 16 ];
    int blocks;
    int fuel_color = color_green;
    int life_region = RG_PLAYER_BLACK;
    int cell = 20;
    int shade_top = dim_color( color_white, 2 );
    int shade_low = dim_color( color_white, 3 );

    /* top strip: a row of solid BIOS blocks (region 20), dimmed */
    for( i = 0; i < 64; ++i )
        draw_bios_block( i * 10, 0, 20, shade_top );

    set_multiply_color( color_white );
    print_at( 8, 0, "SCORE" );
    format_int( g_game.score, buf );
    print_at( 68, 0, buf );

    print_at( 148, 0, "HI" );
    format_int( max2( g_hiscore, g_game.score ), buf );
    print_at( 178, 0, buf );

    print_at( 258, 0, "SPD" );
    format_int( g_game.speed, buf );
    print_at( 298, 0, buf );

    print_at( 330, 0, "LV" );
    format_int( g_game.level, buf );
    print_at( 360, 0, buf );

    /* fuel gauge: the solid BIOS block fills, the lighter shade (region 18)
     * is the empty part; the whole gauge tints red when low */
    blocks = g_game.fuel / 10;
    if( blocks > 10 )
        blocks = 10;
    fuel_color = g_game.fuel > 30 ? color_green : color_red;
    for( i = 0; i < 10; ++i )
    {
        cell = i < blocks ? 20 : 18;     /* ternary to a bare identifier */
        draw_bios_block( 400 + i * 10, 0, cell, fuel_color );
    }
    print_at( 510, 0, "FUEL" );

    /* a light-shade divider row under the strip (region 17, lightest) */
    for( i = 0; i < 64; ++i )
        draw_bios_block( i * 10, 20, 17, shade_low );

    /* lives: the player's own car at half scale.
     * [COV] comma operator in BOTH the init and the update clause of a for
     * (each is split into plain statements for Vircon32 C) */
    life_region = g_game.spy_white ? RG_PLAYER_WHITE : RG_PLAYER_BLACK;
    select_texture( TEX_SPRITES );
    select_region( life_region );
    set_drawing_scale( 0.5f, 0.5f );
    for( i = 0, lx = 560; i < g_game.lives; ++i, lx += 16 )
        draw_region_zoomed_at( lx, 30 );
    set_drawing_scale( 1.0f, 1.0f );
}

/* =============================================================================
 * per-phase frames (dispatched through the g_phase_frame table)
 * ========================================================================== */
void title_frame()
{
    int i;
    int pick = g_game.spy_white;         /* ternary handled through a local,
                                            never a member assignment */
    int* msg_spy;
    int msg_white[ 16 ] = "WHITE SPY";   /* [COV] local int array initialized
                                            from a string literal */
    int msg_black[ 16 ] = "BLACK SPY";
    int preview = RG_PLAYER_BLACK;
    int buf[ 16 ];

    /* slow idle scroll behind the menu */
    g_game.world_y += 2.0f;
    g_game.frame += 1;
    draw_road();

    /* banner from the BIOS shade cells: 19/18 gradient framed by solid 20 */
    for( i = 0; i < 64; ++i )
    {
        draw_bios_block( i * 10, 28, 20, color_black );
        draw_bios_block( i * 10, 48, 19, dim_color( color_white, 1 ) );
        draw_bios_block( i * 10, 68, 18, dim_color( color_white, 1 ) );
        draw_bios_block( i * 10, 88, 20, color_black );
    }

    set_multiply_color( color_white );
    print_center( 36, "SPY VS SPY" );
    set_multiply_color( color_red );
    print_center( 96, "ROAD WARS" );
    set_multiply_color( color_white );

    print_at( 230, 116, "HI-SCORE" );
    format_int( g_hiscore, buf );
    print_at( 320, 116, buf );
    print_center( 126 + 4, BUILD_LABEL );

    /* spy select: either direction flips your spy (once per press) */
    if( g_pad.left_edge != 0 || g_pad.right_edge != 0 )
    {
        pick = pick == 1 ? 0 : 1;        /* ternary to a bare identifier */
        g_game.spy_white = pick;
    }
    msg_spy = msg_black;                 /* array decays to int* */
    msg_spy = g_game.spy_white ? msg_white : msg_black;
    print_at( 190, 152, "<" );
    print_center( 152, msg_spy );
    print_at( 440, 152, ">" );

    preview = g_game.spy_white ? RG_PLAYER_WHITE : RG_PLAYER_BLACK;
    draw_sprite_scaled( preview, 320, 236, 3 );

    if( ( g_game.frame >> 4 ) & 1 )     /* [COV] >> and & on one line */
        print_center( 302, "PRESS START TO RACE" );
    /* [COV] adjacent string-literal concatenation */
    print_center( 328, "A: SHOOT   " "PAD: STEER   " "START: PAUSE" );

    if( g_pad.start_edge != 0 )
        start_round();
}

void play_frame()
{
    /* run every live actor through its kind's update function:
     * [COV] call through an indexed array of function pointers */
    Actor* it  = g_actors;
    Actor* end = g_actors + MAX_ACTORS;
    int lvl;

    g_game.frame += 1;
    g_game.pace = 3 + g_game.speed;
    if( g_game.flat_tire > 0 )
        g_game.pace = g_game.pace / 2 + 1;

    while( it != end )
    {
        if( it->alive )
            g_kind_update[ it->kind ]( it );
        ++it;
    }

    if( g_game.invuln > 0 )
        g_game.invuln -= 1;

    collide_player();

    /* world scroll: float accumulator (0 if we crashed this frame) */
    g_game.world_y += (float)g_game.pace;

    /* difficulty ramp: a new level every LEVEL_LENGTH distance units */
    lvl = clampi( 1 + g_game.distance / LEVEL_LENGTH, 1, MAX_LEVEL );
    if( lvl > g_game.level )
    {
        g_game.level = lvl;
        g_game.banner_timer = 90;
    }

    /* spawner pacing tightens with speed and level */
    {
        int gap = 56 - g_game.speed * 4 - g_game.level * 3;
        gap = gap < 16 ? 16 : gap;       /* ternary to a bare identifier */
        g_game.spawn_timer += 1;
        if( g_game.spawn_timer >= gap )
        {
            g_game.spawn_timer = 0;
            spawn_wave();
        }
    }

    /* keep the frame counter from overflowing in a long session */
    if( g_game.frame > 100000 )
        g_game.frame = 0;

    draw_road();
    draw_actors();
    draw_hud();

    if( g_game.banner_timer > 0 )
    {
        g_game.banner_timer -= 1;
        if( ( g_game.banner_timer >> 3 ) & 1 )
        {
            set_multiply_color( color_red );
            print_center( 100, "LEVEL UP" );
            set_multiply_color( color_white );
        }
    }

    if( g_pad.start_edge != 0 && g_game.phase == PH_PLAY )
        g_game.phase = PH_PAUSE;
}

void pause_frame()
{
    int i;
    draw_road();
    draw_actors();
    draw_hud();
    for( i = 0; i < 24; ++i )
    {
        draw_bios_block( 200 + i * 10, 150, 20, color_black );
        draw_bios_block( 200 + i * 10, 170, 20, color_black );
    }
    set_multiply_color( color_white );
    print_center( 152, "PAUSED" );
    print_center( 172, "START: RESUME" );
    if( g_pad.start_edge != 0 )
        g_game.phase = PH_PLAY;
}

void crash_frame()
{
    /* freeze the world; effects play out */
    int i;
    for( i = 0; i < MAX_ACTORS; ++i )
        if( g_actors[ i ].alive && g_actors[ i ].kind == KIND_EFFECT )
            update_effect( &g_actors[ i ] );

    g_game.crash_timer -= 1;
    draw_road();
    draw_actors();
    draw_hud();

    if( g_game.crash_timer <= 0 )
    {
        if( g_game.lives > 0 )
        {
            respawn_player();
            g_game.phase = PH_PLAY;
        }
        else
        {
            g_new_hi = g_game.score > g_hiscore;
            g_hiscore = max2( g_hiscore, g_game.score );
            g_game.phase = PH_OVER;
        }
    }
}

void over_frame()
{
    int buf[ 16 ];
    int msg_retry[ 24 ] = "PRESS START TO RETRY";  /* local string-array init */

    clear_screen( color_black );
    set_multiply_color( color_red );
    print_center( 80, g_msg_over );
    set_multiply_color( color_white );
    print_center( 130, "FINAL SCORE" );
    format_int( g_game.score, buf );
    print_center( 155, buf );
    print_center( 190, "DISTANCE" );
    format_int( g_game.distance, buf );
    print_center( 215, buf );
    if( g_new_hi != 0 )
    {
        if( ( g_game.frame >> 3 ) & 1 )
            print_center( 260, "NEW HI-SCORE!" );
    }
    else
    {
        print_center( 250, "HI-SCORE" );
        format_int( g_hiscore, buf );
        print_center( 275, buf );
    }
    if( ( g_game.frame >> 4 ) & 1 )
        print_center( 320, msg_retry );
    g_game.frame += 1;
}

/* =============================================================================
 * round / boot
 * ========================================================================== */
void enter_title()
{
    clear_actors();
    g_player = 0;
    g_game.phase = PH_TITLE;
    g_game.frame = 0;
    g_game.world_y = 0.0f;
}

void start_round()
{
    clear_actors();

    g_game.phase = PH_PLAY;
    g_game.score = 0;
    g_game.lives = START_LIVES;
    g_game.fuel = FUEL_MAX;
    g_game.speed = 1;
    g_game.pace = 4;
    g_game.level = 1;
    g_game.distance = 0;
    g_game.frame = 0;
    g_game.invuln = 60;
    g_game.crash_timer = 0;
    g_game.spawn_timer = 30;
    g_game.flat_tire = 0;
    g_game.fire_cooldown = 0;
    g_game.shift_timer = 0;
    g_game.fuel_timer = 0;
    g_game.banner_timer = 0;
    g_game.world_y = 0.0f;
    g_new_hi = 0;

    /* the player is always slot 0 (the pool was just cleared) */
    spawn_actor( KIND_PLAYER, LANE_X( 1 ), PLAYER_Y, 18, 32,
                 g_game.spy_white ? RG_PLAYER_WHITE : RG_PLAYER_BLACK, 0 );
    g_player = &g_actors[ 0 ];
}

void boot_game()
{
    int octal_seed = 0273;    /* [COV] octal literal */

    srand( get_frame_counter() ^ ( octal_seed << 8 ) ^ sizeof( Actor ) );
    setup_regions();
    set_blending_mode( blending_alpha );
    set_multiply_color( color_white );
    select_texture( TEX_SPRITES );

    /* [COV] function-pointer table slots assigned from bare function names
     * (implicit &, Vircon32-form declarators in the generated C) */
    g_kind_update[ KIND_NONE ]     = update_none;
    g_kind_update[ KIND_PLAYER ]   = update_player;
    g_kind_update[ KIND_TRAFFIC ]  = update_traffic;
    g_kind_update[ KIND_RIVAL ]    = update_rival;
    g_kind_update[ KIND_BULLET ]   = update_bullet;
    g_kind_update[ KIND_OBSTACLE ] = update_obstacle;
    g_kind_update[ KIND_EFFECT ]   = update_effect;
    g_kind_update[ KIND_PROP ]     = update_prop;

    g_phase_frame[ PH_TITLE ] = title_frame;
    g_phase_frame[ PH_PLAY ]  = play_frame;
    g_phase_frame[ PH_CRASH ] = crash_frame;
    g_phase_frame[ PH_OVER ]  = over_frame;
    g_phase_frame[ PH_PAUSE ] = pause_frame;

    g_game.spy_white = 0;
    g_hiscore = 0;

    /* [COV] the Vircon32-NATIVE function-pointer declarator spelling,
     * accepted on input alongside the standard one used for the tables.
     * The parameter list matches update_traffic, so this is type-correct C. */
    {
        void ( Actor* )* fn_check;
        fn_check = update_traffic;
        fn_check( &g_actors[ MAX_ACTORS - 1 ] );   /* once, on a dead slot: a no-op */
    }
}

/* =============================================================================
 * main -- declared int main() on purpose: the transpiler must force
 * void main(void) and strip this return. The goto below is the one
 * deliberate jump: a flat-scope backward jump with no locals to unwind,
 * taken from the game-over screen back to the title.
 * ========================================================================== */
int main()
{
    boot_game();

back_to_title:
    enter_title();

    while( 1 )
    {
        poll_input();

        /* [COV] dispatch through an array of function pointers */
        g_phase_frame[ g_game.phase ]();

        end_frame();

        if( g_game.phase == PH_OVER && g_pad.start_edge != 0 )
            goto back_to_title;    /* [COV] goto + label, flat scope */
    }

    return 0;    /* unreachable: stripped by the Vircon32-mode rewrite */
}
