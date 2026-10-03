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
 *   2. Stress the Vircon32-rewrite passes: ternaries appear ONLY in the three
 *      rewritten positions (variable init / bare-identifier assignment /
 *      return); comma operators ONLY in the split positions (expression
 *      statements and for clauses); structs travel by pointer only
 *      (one-word ABI); function pointers in BOTH declarator spellings;
 *      arrays in BOTH declarator spellings.
 *
 * Companion files:
 *   - sprites.png   : the single texture referenced by the #texture hint below.
 *   - gap_probes.cpp: constructs KNOWN-unsupported today, each gated behind
 *                     its own -DGAP_xxx macro so failures stay isolated.
 *
 * BUILD:
 *   v32c++ -o spyroad.c spyroad.cpp
 *   then compile spyroad.c with the Vircon32 C compiler and pack the cart
 *   with the generated spyroad.xml. Keep sprites.png beside this source.
 *
 * STYLE NOTES:
 *   - Plain C89-flavoured C (the transpiler's C-subset-of-C++). It
 *     deliberately avoids: static/extern/volatile, unsigned, bit-fields,
 *     const-pointers, `~`, `%=`, struct by-value, ternaries and comma
 *     operators outside their rewritten/split positions.
 *   - main is declared `int main()` on purpose: the transpiler must force
 *     `void main(void)` and strip the trailing return.
 * ========================================================================== */

/* ----------------------------------------------------------------------------
 * Cart hints (recognized by v32c++'s lexer; drive the generated XML)
 * -------------------------------------------------------------------------- */
#texture TEX_SPRITES "textures/spyvsspy_sprites.png"
#title "Spy vs Spy: Road Wars"
#version 1.0

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
/* Indented directives: [FOUND GAP #1] -- used to die with "syntax error,
 * unexpected invalid token" because lexer.l's '#' rules were column-0
 * anchored while prescan.c (correctly) accepts and re-emits indented
 * directives. FIXED in lexer.l (leading [ \t]* allowed on all '#' rules);
 * the indentation above is kept deliberately as a regression test for it. */

/* ----------------------------------------------------------------------------
 * Tunables. Function-like macros with parenthesized bodies.
 * -------------------------------------------------------------------------- */
#define ROAD_LEFT     128
#define ROAD_RIGHT    512
#define LANE_COUNT    3
#define LANE_WIDTH    (( ROAD_RIGHT - ROAD_LEFT ) / LANE_COUNT)   /* 128 */
#define LANE_X( l )   ( ROAD_LEFT + LANE_WIDTH/2 + (l) * LANE_WIDTH )

#define TILE          32
#define MAX_ACTORS    24
#define PATTERN_COUNT 8
#define FUEL_MAX      100
#define START_LIVES   3
#define PLAYER_Y      282

/* actor flag bits: bitwise coverage runs through the whole game logic */
#define F_WHITE   1     /* reserved: white-spy variant sprite        */
#define F_OIL     2     /* sliding on oil                            */
#define F_SPIKES  4     /* flat tires, speed halved                  */
#define F_FUEL    8     /* this obstacle is a fuel can               */
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
    KIND_RIVAL   = 3,
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
    PH_COUNT       /* 4 */
};

/* ----------------------------------------------------------------------------
 * [COV] plain C structs (one-word members; never passed or returned by
 *      value -- the Vircon32 C ABI takes single-word parameters/returns)
 * -------------------------------------------------------------------------- */
struct Actor
{
    int kind;
    int x;
    int y;
    int vx;
    int vy;
    int w;
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
    int distance;
    int frame;
    int invuln;
    int crash_timer;
    int spawn_timer;
    int flat_tire;
    int spy_white;
    float world_y;
};

/* [FOUND GAP #2] `typedef struct Actor Actor;` does NOT parse -- that was
 * the "syntax error, unexpected STRUCT" failure. typedef_decl only accepts
 * a type_spec (int/float/void/bool/char/TYPE_NAME/qualified/const) after
 * TYPEDEF, and the struct/union/enum KEYWORDS are not in type_spec's first
 * set. Elaborated type specifiers are a gap everywhere, not just in
 * typedefs: `struct Actor x;` as a declaration fails the same way. The
 * typedefs are unnecessary anyway: class_decl/union_decl register the tag
 * in the symbol table, and the lexer hack classifies SYM_CLASS/SYM_UNION/
 * SYM_ENUM/SYM_TYPEDEF names as TYPE_NAME, so the bare names Actor and
 * GameState (and ShadeWord below) are already usable as types. */

/* [COV] function-pointer typedefs in the STANDARD-C declarator spelling;
 * the generated C must come out in Vircon32's reversed form regardless.
 *
 * [FOUND GAP #3] the original `typedef void (*ActorFn)( Actor* a );`
 * -- parameter NAMED -- died with "syntax error, unexpected IDENTIFIER,
 * expecting ')'". func_ptr_param_type accepts only bare types
 * (`type_spec pointer_opt`), so after `Actor*` the only legal tokens are
 * ',' and ')'. The grammar comment calls that deliberate and claims it
 * "matches real C++ exactly" -- but real C/C++ allows a parameter name
 * in ANY function declarator, including fp typedefs (it is parsed and
 * discarded), so this is a true coverage gap, not a scope boundary.
 * Workaround: drop the name. Probe: -DGAP_FP_PARAM_NAME. */
typedef void (*ActorFn)( Actor* );
typedef void (*FrameFn)();

/* ----------------------------------------------------------------------------
 * [COV] union with an array member (used to tint colours channel-wise)
 * -------------------------------------------------------------------------- */
union ShadeWord
{
    int word;
    int channel[ 4 ];
};

/* ----------------------------------------------------------------------------
 * Globals.
 * [COV] file-scope variables; braced initializer lists; binary literals;
 *       a string literal as an int-array initializer; the Vircon32-native
 *       length-before-name array declarator, accepted interchangeably.
 * -------------------------------------------------------------------------- */
Actor     g_actors[ MAX_ACTORS ];
Actor*    g_player;                      /* assigned in start_round */
GameState g_game;

ActorFn g_kind_update[ KIND_COUNT ];     /* [COV] enum constant as array dim */
FrameFn g_phase_frame[ PH_COUNT ];

/* standard declarator + braced init list of binary literals */
int g_pattern_bits[ PATTERN_COUNT ] = { 0b0001, 0b0010, 0b0100, 0b1000,
                                        0b0011, 0b0110, 0b1100, 0b0101 };

/* Vircon32-native declarator spelling (as the SDK's own headers write it) */
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
void update_none( Actor* a );
int  g_player_x();
void crash_player();
void spawn_effect( int x, int y, int big );
void start_round();
void title_frame();
void play_frame();
void crash_frame();
void over_frame();

/* =============================================================================
 * small helpers
 * ========================================================================== */

/* [COV] inline (accepted, ignored) + const-qualified parameters + a ternary
 * CHAIN in a return expression (rewritten position #3) */
inline int clampi( const int v, const int lo, const int hi )
{
    return v < lo ? lo : ( v > hi ? hi : v );
}

/* [COV] ternary in a return expression */
int max2( int a, int b )
{
    return a > b ? a : b;
}

/* [COV] union member access + array subscripting on a union member + shifts */
int dim_color( int color, int shift )
{
    ShadeWord u;      /* [GAP #2] bare tag name: `union ShadeWord u;`
                         would fail -- the UNION keyword cannot start a
                         local var_decl, only a top-level union_decl */
    int c;
    u.word = color;
    for( c = 0; c < 3; ++c )
        u.channel[ c ] = u.channel[ c ] >> shift;
    return u.word;
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

/* scale 0 = plain draw; scale >= 2 = centred zoom (title preview etc.) */
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

/* returns the slot index, or -1 when the pool is full. An int return keeps
 * this file off the pointer-return path entirely; NULL/0 pointer business
 * belongs to gap_probes.cpp, not here */
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
 * needed: `sizeof *a`), plus memset from a pass-through header */
void kill_actor( Actor* a )
{
    memset( a, 0, sizeof *a );
    a->alive = false;
}

void spawn_effect( int x, int y, int big )
{
    int i;
    int spread = 10;    /* [COV] ternary directly initializing a local
                           (rewritten position #1) */
    int step = big ? 6 : 3;
    if( big )
        spread = 18;
    for( i = 0; i < 3; ++i )
    {
        int slot = spawn_actor( KIND_EFFECT, x, y, 16, 16, RG_EXPLO_C, 0 );
        if( slot >= 0 )
        {
            g_actors[ slot ].life = 24 - i * 6;
            g_actors[ slot ].x    = x + ( i - 1 ) * spread;
            g_actors[ slot ].y    = y + ( i - 1 ) * step;
        }
    }
    {
        int slot = spawn_actor( KIND_EFFECT, x, y - 10, 16, 16, RG_SMOKE, 0 );
        if( slot >= 0 )
            g_actors[ slot ].life = 30;
    }
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

/* =============================================================================
 * per-kind updates (dispatched through the g_kind_update table)
 * ========================================================================== */
void update_none( Actor* a )
{
    /* KIND_NONE and static KIND_OBSTACLE: obstacles ride the road, so their
     * motion happens against the scrolling world, not on their own.
     * Touch nothing; self-assign keeps this a genuine statement. */
    a->flags = a->flags & 0;
}

void update_player( Actor* a )
{
    int dx;
    int dy;
    int pace = 3 + g_game.speed;
    int steer = 4;

    /* flat tires from the spike strip: half pace, mushy steering */
    if( g_game.flat_tire > 0 )
    {
        g_game.flat_tire -= 1;
        pace = pace / 2 + 1;
        steer = 2;
    }

    /* [COV] pointer out-params into an SDK function */
    gamepad_direction( &dx, &dy );

    /* oil slick: steering becomes a slide; drift carries the last vx */
    if( ( a->flags & F_OIL ) != 0 )   /* [COV] & with explicit parens --
                                         C's `a & b == c` binds == tighter */
    {
        if( a->life > 0 )
        {
            a->life -= 1;
            a->x += a->vx * 3;
            if( ( g_game.frame & 3 ) == 0 )
            {
                int skid = spawn_actor( KIND_EFFECT, a->x, a->y + 26,
                                        32, 16, RG_SKID, 0 );
                if( skid >= 0 )
                    g_actors[ skid ].life = 10;
            }
        }
        else
        {
            a->flags &= F_ALL ^ F_OIL;    /* clear the oil bit, no `~` needed */
        }
    }
    else
    {
        /* [COV] ternary directly assigned to a bare identifier
         * (rewritten position #2) */
        steer = ( dy < 0 ) ? steer + 2 : steer;
        a->vx = dx * steer;
        a->x += a->vx;
        /* gas / brake on the same pad */
        g_game.speed = clampi( g_game.speed + dy, 0, 5 );
    }

    /* keep the whole car on asphalt */
    a->x = clampi( a->x, ROAD_LEFT + 14, ROAD_RIGHT - 14 );

    /* frame tick + fuel burn scaling with speed */
    g_game.frame += 1;
    if( ( g_game.frame & 3 ) == 0 )
    {
        g_game.fuel -= 1 + g_game.speed / 2;
        if( g_game.fuel <= 0 )
        {
            g_game.fuel = 0;
            crash_player();
            return;
        }
    }

    /* distance and score tick up with pace */
    g_game.distance += pace;
    if( ( g_game.frame & 7 ) == 0 )
        g_game.score += 1 + g_game.speed / 2;

    /* shooting: button A, at most every 8 frames */
    if( gamepad_button_a() > 0 && ( g_game.frame & 7 ) == 0 )
    {
        int slot = spawn_actor( KIND_BULLET, a->x, a->y - 24, 6, 12,
                                RG_BULLET_PLY, 0 );
        if( slot >= 0 )
        {
            g_actors[ slot ].vy = -( pace + 9 );      /* [COV] unary minus */
            spawn_actor( KIND_EFFECT, a->x, a->y - 30, 8, 8, RG_MUZZLE, 0 );
        }
    }

    /* world scroll: float accumulator, int use */
    g_game.world_y += (float)pace;
}

void update_traffic( Actor* a )
{
    /* traffic moves slower than the player, so it drifts down the screen */
    int pace = 3 + g_game.speed;
    a->y += pace - 2;
    if( a->y > screen_height + 48 )
        a->alive = false;
}

void update_rival( Actor* a )
{
    int pace = 3 + g_game.speed;
    int target = g_player_x();

    /* steer toward the player's lane */
    if( a->x < target - 10 )
        a->vx = 2;
    else if( a->x > target + 10 )
        a->vx = -2;
    else
        a->vx = 0;
    a->x = clampi( a->x + a->vx, ROAD_LEFT + 14, ROAD_RIGHT - 14 );

    /* catch up while far above, then pull alongside */
    if( a->y < PLAYER_Y - 130 )
        a->y -= 3;
    else
        a->y += pace - 2;

    if( a->y > screen_height + 48 )
        a->alive = false;
}

void update_bullet( Actor* a )
{
    Actor* it  = g_actors;
    Actor* end = g_actors + MAX_ACTORS;
    a->y += a->vy;
    if( a->y < -20 || a->y > screen_height + 20 )
    {
        a->alive = false;
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
                /* bullets fly over oil and spikes */
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
                            int bounty = 40;    /* ternary to a bare
                                                  identifier, never straight
                                                  into a member assign */
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
    if( a->region == RG_SMOKE )
        return;                          /* smoke lingers, no cycling */
    if( a->region == RG_MUZZLE )
    {
        a->alive = false;                /* one-frame flash */
        return;
    }
    if( a->region == RG_SKID )
    {
        a->y += 3;                       /* drift with the road */
        return;
    }
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
    int pace = 3 + g_game.speed;
    a->y += pace;
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
    g_game.phase = PH_CRASH;
    g_game.crash_timer = 46;
    g_game.lives -= 1;
    if( g_player != 0 )
        spawn_effect( g_player->x, g_player->y, true );
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
                        g_player->flags |= F_OIL;
                    else
                    {
                        g_player->flags |= F_SPIKES;
                        g_game.flat_tire = 90;
                    }
                    g_player->life = 30;         /* slide timer */
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
 * spawning director
 * ========================================================================== */
void spawn_wave()
{
    int pick = rand() & ( PATTERN_COUNT - 1 );    /* [COV] rand + & mask */
    int bits = g_pattern_bits[ pick ];
    int lane;
    int rival_region = RG_RIVAL_BLACK;   /* [COV] ternary assigned to a bare
                                            identifier (rewritten position #2) */
    rival_region = g_game.spy_white ? RG_RIVAL_WHITE : RG_RIVAL_BLACK;

    /* rivals drive the spy you did NOT pick */
    if( ( rand() & 3 ) == 0 )
    {
        spawn_actor( KIND_RIVAL, LANE_X( rand() % LANE_COUNT ), -48,
                     24, 40, rival_region, 0 );
        return;
    }

    for( lane = 0; lane < LANE_COUNT; ++lane )
    {
        if( ( bits & ( 1 << lane ) ) != 0 )       /* [COV] variable shift */
        {
            int what = rand() & 3;
            int region = RG_BARREL;               /* ternary in a local init
                                                     (rewritten position #1) */
            region = what == 0 ? RG_OIL : ( what == 1 ? RG_CRATE : RG_BARREL );
            if( what == 3 )
                region = RG_FUELCAN;
            spawn_actor( KIND_OBSTACLE, LANE_X( lane ),
                         -48 - lane * 24, 32, 24, region, 0 );
        }
    }

    /* neutral traffic shares the road */
    if( ( rand() & 1 ) != 0 )
    {
        int traffic_region = RG_TRAFFIC_GRAY;     /* chained ternary to a bare
                                                     identifier */
        traffic_region = ( rand() & 7 ) == 0 ? RG_TRUCK
                         : ( ( rand() & 1 ) ? RG_TRAFFIC_BROWN
                                            : RG_TRAFFIC_GRAY );
        spawn_actor( KIND_TRAFFIC, LANE_X( rand() % LANE_COUNT ), -56,
                     24, 40, traffic_region, 0 );
    }

    /* roadside dressing on the grass */
    if( ( rand() & 7 ) == 0 )
    {
        int prop_region = RG_LAMPPOST;            /* ternary to bare ident */
        prop_region = ( rand() & 1 ) ? RG_SIGN
                       : ( ( rand() & 1 ) ? RG_TREE : RG_LAMPPOST );
        {
            int prop_x = 64;
            prop_x = ( rand() & 1 ) ? 576 : 64;
            spawn_actor( KIND_PROP, prop_x, -48, 24, 32, prop_region, 0 );
        }
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
    int row_off = ( (int)g_game.world_y ) % TILE;
    int base_y = row_off - TILE;

    select_texture( TEX_SPRITES );
    for( row = 0; row <= 12; ++row )
    {
        int y = base_y + row * TILE;
        for( col = 0; col < 20; ++col )
        {
            int region;
            /* [COV] switch with a fall-through label stack + default */
            switch( col )
            {
                case 4:
                    region = RG_ROAD_EDGE_L;
                    break;
                case 5:
                case 6:
                case 8:
                case 9:
                case 11:
                case 12:
                    region = RG_ASPHALT;
                    break;
                case 7:
                case 10:
                    region = RG_ASPHALT_DASH;
                    break;
                case 13:
                    region = RG_ROAD_EDGE_R;
                    break;
                default:
                    region = RG_GRASS;
                    break;
            }
            /* sprinkle bushes on the grass */
            if( region == RG_GRASS )
            {
                int weave = ( col * 7 + row * 13 + g_game.frame / 6 ) % 11;
                if( weave == 0 )
                    region = RG_GRASS_BUSH;
            }
            select_region( region );
            draw_region_at( col * TILE, y );
        }
    }
}

void draw_actor( Actor* a )
{
    select_texture( TEX_SPRITES );
    select_region( a->region );
    draw_region_at( a->x, a->y );
}

void draw_player()
{
    int region = RG_PLAYER_BLACK;      /* ternary to a bare identifier */
    bool skip = false;
    if( g_player == 0 )
        return;
    region = g_game.spy_white ? RG_PLAYER_WHITE : RG_PLAYER_BLACK;
    skip = g_game.invuln > 0 && ( g_game.frame & 2 ) != 0;
    if( skip )
        return;                        /* blink while invulnerable */
    draw_sprite_scaled( region, g_player->x, g_player->y, 1 );
}

/* [COV] a for loop with comma-separated init AND update clauses (both get
 * split into plain statements for Vircon32 C) */
void draw_actors()
{
    int i, j;
    for( i = 0, j = MAX_ACTORS - 1; i <= j; ++i, --j )
    {
        if( g_actors[ i ].alive )
            draw_actor( &g_actors[ i ] );
        if( i != j && g_actors[ j ].alive )
            draw_actor( &g_actors[ j ] );
    }
}

void draw_hud()
{
    int i;
    int buf[ 16 ];
    int blocks;
    int fuel_color = color_green;
    int life_region = RG_PLAYER_BLACK;    /* ternary to a bare identifier */
    int cell = 20;

    /* top strip: a row of solid BIOS blocks (region 20), dimmed */
    for( i = 0; i < 64; ++i )
        draw_bios_block( i * 10, 0, 20, dim_color( color_white, 2 ) );

    /* score */
    set_multiply_color( color_white );
    print_at( 12, 0, "SCORE" );
    format_int( g_game.score, buf );
    print_at( 72, 0, buf );

    /* speed */
    print_at( 200, 0, "SPEED" );
    format_int( g_game.speed, buf );
    print_at( 260, 0, buf );

    /* fuel gauge: the solid BIOS block fills, the lighter shade (region 18)
     * is the empty part; the whole gauge tints red when low */
    blocks = g_game.fuel / 10;
    if( blocks > 10 )
        blocks = 10;
    fuel_color = g_game.fuel > 30 ? color_green : color_red;
    for( i = 0; i < 10; ++i )
    {
        cell = i < blocks ? 20 : 18;     /* ternary to a bare identifier */
        draw_bios_block( 360 + i * 10, 0, cell, fuel_color );
    }
    print_at( 470, 0, "FUEL" );

    /* a light-shade divider row under the strip (region 17, lightest) */
    for( i = 0; i < 64; ++i )
        draw_bios_block( i * 10, 20, 17, dim_color( color_white, 3 ) );

    /* lives: the player's own car at half scale */
    life_region = g_game.spy_white ? RG_PLAYER_WHITE : RG_PLAYER_BLACK;
    for( i = 0; i < g_game.lives; ++i )
    {
        select_region( life_region );
        set_drawing_scale( 0.5f, 0.5f );
        draw_region_zoomed_at( 560 + i * 16, 30 );
    }
    set_drawing_scale( 1.0f, 1.0f );
}

/* =============================================================================
 * per-phase frames (dispatched through the g_phase_frame table)
 * ========================================================================== */
void title_frame()
{
    /* slow idle scroll behind the menu */
    int i;
    int pick = g_game.spy_white;         /* ternary handled through a local,
                                           never a member assignment */
    int* msg_spy;
    int msg_white[ 16 ] = "WHITE SPY";   /* [COV] local int array initialized
                                           from a string literal */
    int msg_black[ 16 ] = "BLACK SPY";
    int preview = RG_PLAYER_BLACK;      /* ternary to a bare identifier */

    g_game.world_y += 2.0f;
    draw_road();
    draw_actors();

    /* banner from the BIOS shade cells: 19/18 gradient framed by solid 20 */
    for( i = 0; i < 64; ++i )
    {
        draw_bios_block( i * 10, 28, 20, color_black );
        draw_bios_block( i * 10, 48, 19, dim_color( color_white, 1 ) );
        draw_bios_block( i * 10, 68, 18, dim_color( color_white, 1 ) );
        draw_bios_block( i * 10, 88, 20, color_black );
    }

    set_multiply_color( color_white );
    print_at( 270, 36, "SPY VS SPY" );
    set_multiply_color( color_red );
    print_at( 275, 96, "ROAD WARS" );
    set_multiply_color( color_white );
    print_at( 140, 132, BUILD_LABEL );

    /* spy select: left or right flips your spy */
    if( gamepad_left() > 0 || gamepad_right() > 0 )
    {
        pick = pick == 1 ? 0 : 1;        /* ternary to a bare identifier */
        g_game.spy_white = pick;
    }
    msg_spy = msg_black;                 /* array decays to int* */
    msg_spy = g_game.spy_white ? msg_white : msg_black;
    print_at( 208, 160, msg_spy );

    preview = g_game.spy_white ? RG_PLAYER_WHITE : RG_PLAYER_BLACK;
    draw_sprite_scaled( preview, 320, 220, 3 );

    if( ( g_game.frame >> 4 ) & 1 )     /* [COV] >> and & on one line */
        print_at( 235, 300, "PRESS START TO RACE" );
    /* [COV] adjacent string-literal concatenation */
    print_at( 250, 322, "A: SHOOT   " "PAD: STEER" );

    g_game.frame += 1;
    if( gamepad_button_start() > 0 )
        start_round();
}

void play_frame()
{
    /* run every live actor through its kind's update function:
     * [COV] call through an indexed array of function pointers */
    Actor* it  = g_actors;
    Actor* end = g_actors + MAX_ACTORS;
    while( it != end )
    {
        if( it->alive )
            g_kind_update[ it->kind ]( it );
        ++it;
    }

    if( g_game.invuln > 0 )
        g_game.invuln -= 1;

    collide_player();

    /* spawner pacing tightens with speed */
    {
        int pace = 52 - g_game.speed * 5;
        pace = pace < 16 ? 16 : pace;    /* ternary to a bare identifier */
        g_game.spawn_timer += 1;
        if( g_game.spawn_timer >= pace )
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
    draw_player();
    draw_hud();
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
        int next = PH_OVER;              /* ternary to a bare identifier */
        next = g_game.lives > 0 ? PH_PLAY : PH_OVER;
        g_game.phase = next;
        if( next == PH_PLAY )
        {
            g_game.invuln = 90;
            g_game.fuel = FUEL_MAX;
        }
    }
}

void over_frame()
{
    int buf[ 16 ];
    int msg_retry[ 24 ] = "PRESS START TO RETRY";  /* local string-array init */

    clear_screen( color_black );
    set_multiply_color( color_white );
    print_at( 240, 120, g_msg_over );
    print_at( 230, 160, "FINAL SCORE" );
    format_int( g_game.score, buf );
    print_at( 300, 190, buf );
    if( ( g_game.frame >> 4 ) & 1 )
        print_at( 220, 240, msg_retry );
    g_game.frame += 1;
}

/* =============================================================================
 * round / boot
 * ========================================================================== */
void start_round()
{
    int i;
    /* [COV] sizeof an array expression + memset from a pass-through header */
    memset( g_actors, 0, sizeof g_actors );
    for( i = 0; i < MAX_ACTORS; ++i )
        g_actors[ i ].alive = false;

    g_game.phase = PH_PLAY;
    g_game.score = 0;
    g_game.lives = START_LIVES;
    g_game.fuel = FUEL_MAX;
    g_game.speed = 1;
    g_game.distance = 0;
    g_game.frame = 0;
    g_game.invuln = 60;
    g_game.crash_timer = 0;
    g_game.spawn_timer = 30;
    g_game.flat_tire = 0;
    g_game.world_y = 0.0f;

    spawn_actor( KIND_PLAYER, LANE_X( 1 ), PLAYER_Y, 24, 40,
                 RG_PLAYER_BLACK, 0 );
    g_player = &g_actors[ 0 ];
}

void boot_game()
{
    int octal_seed = 0273;    /* [COV] octal literal */
    int i;

    srand( get_frame_counter() ^ ( octal_seed << 8 ) ^ sizeof( Actor ) );
    setup_regions();
    set_blending_mode( blending_alpha );
    set_multiply_color( color_white );
    select_texture( TEX_SPRITES );

    /* [COV] function-pointer table slots assigned from bare function names
     * (implicit &, Vircon32-form declarators in the generated C) */
    g_kind_update[ KIND_NONE ]     = update_none;
    g_kind_update[ KIND_PLAYER ]    = update_player;
    g_kind_update[ KIND_TRAFFIC ]  = update_traffic;
    g_kind_update[ KIND_RIVAL ]    = update_rival;
    g_kind_update[ KIND_BULLET ]   = update_bullet;
    g_kind_update[ KIND_OBSTACLE ] = update_none;    /* static: rides the road */
    g_kind_update[ KIND_EFFECT ]   = update_effect;
    g_kind_update[ KIND_PROP ]     = update_prop;

    g_phase_frame[ PH_TITLE ] = title_frame;
    g_phase_frame[ PH_PLAY ]  = play_frame;
    g_phase_frame[ PH_CRASH ] = crash_frame;
    g_phase_frame[ PH_OVER ]  = over_frame;

    g_game.phase = PH_TITLE;
    g_game.spy_white = 0;
    g_game.frame = 0;

    /* [COV] the Vircon32-NATIVE function-pointer declarator spelling,
     * accepted on input alongside the standard one used for the tables.
     * [SOURCE FIX, not a transpiler workaround] the param list here used
     * to read `( int )`, which made `fn_check = update_traffic;` a
     * genuine C type error (a `void(int)*` cannot hold a function of
     * type `void(Actor*)`, and the call then passed `Actor*` where `int`
     * was declared) -- gcc rejects that just the same. Matching
     * update_traffic's signature is the CORRECT form, not a concession:
     * it also exercises func_ptr_param_type with a TYPE_NAME + pointer. */
    {
        void ( Actor* )* fn_check;
        fn_check = update_traffic;
        fn_check( &g_actors[ 1 ] );   /* once, on a dead slot: a no-op */
    }

    /* [COV] comma operator in a for INIT clause (split for Vircon32 C);
     * the loop body is a deliberate no-op self-assignment */
    for( i = 0, octal_seed = 0; i < KIND_COUNT; ++i )
        g_pattern_speed[ i % PATTERN_COUNT ] =
            g_pattern_speed[ i % PATTERN_COUNT ];
}

/* =============================================================================
 * main -- declared `int main()` on purpose: the transpiler must force
 * `void main(void)` and strip this return. The goto below is the one
 * deliberate jump: a flat-scope backward jump with no locals to unwind.
 * ========================================================================== */
int main()
{
    boot_game();

restart_round:
    start_round();

    /* let START come back up before the round begins (no instant skip) */
    do
    {
        end_frame();
    } while( gamepad_button_start() > 0 );

    while( 1 )
    {
        /* [COV] dispatch through an array of function pointers */
        g_phase_frame[ g_game.phase ]();

        end_frame();

        if( g_game.phase == PH_OVER && gamepad_button_start() > 0 )
            goto restart_round;    /* [COV] goto + label, flat scope */
    }

    return 0;    /* unreachable: stripped by the Vircon32-mode rewrite */
}
