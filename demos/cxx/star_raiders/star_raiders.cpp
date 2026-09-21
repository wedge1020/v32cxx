#title "Star Raiders"
#version 0.9

// sounds (VSND assets; symbols defined by the #sound cart hints)
#sound SFX_MISSILE    "sounds/missile.wav"
#sound SFX_EXPLOSION  "sounds/explosion.wav"
#sound SFX_BEEP       "sounds/beep.wav"
#sound SFX_HYPERSPACE "sounds/hyperspace.wav"
#sound SFX_ENGINE     "sounds/engine.wav"
#sound SFX_ALERT      "sounds/alert.wav"
#sound SFX_REPLENISH  "sounds/replenish.wav"
#sound SFX_TITLE      "sounds/title.wav"
#sound SFX_GAME       "sounds/gameplay.wav"

#include "video.h"
#include "input.h"
#include "audio.h"
#include "math.h"
#include "string.h"
#include "time.h"

// ============================================================================
//  STAR RAIDERS for Vircon32
//  Phase 1-3: quadrant starfield, Zylons, galactic chart (superseded)
//  Phase 4: first-person pseudo-3D space
//  Phase 5: chart navigation, throttle, shields, attack computer
//  Phase 6: missile physics, reverse gear, asteroid fields,
//           block-built composite sprites for Zylons and starbases
//  Phase 7: aft view on B (mirrored projection, half reticle),
//           debris explosions from block glyphs, safe start quadrant
//  Phase 8: Zylon migration clock, stray asteroids, bulbous rocks
//
//  Written to the v32c++ subset: no templates, no static members, no
//  in-class initializers, no ternaries, no 'unsigned', one-word
//  parameter/return types. Tunables are #defines; text lives in
//  ASCII-code int arrays (the v32c++ grammar has no string-literal
//  array initializers).
//
//  Graphics use ONLY the BIOS font (texture -1, 10x20 glyphs, region
//  id = ASCII code). Hotspots are left at the BIOS default (top-left,
//  as print_at expects); centered drawing is done in screen space.
//  Sprites are composites of the block glyphs 0x11-0x14 (light solid
//  through full solid), the drawing characters the BIOS overlays on
//  the control-character range.
// ============================================================================

// ---------------------------------------------------------------------------
//  Tunables
// ---------------------------------------------------------------------------

#define SCREEN_W   640
#define SCREEN_H   360
#define CENTER_X   320
#define CENTER_Y   180

#define ASCII_DOT   46    // '.'
#define ASCII_PLUS  43    // '+'
#define ASCII_V     86    // 'V' -- chart marker for hostile quadrants
#define ASCII_DASH  45    // '-' -- HUD bars
#define ASCII_STAR  42    // '*' -- missiles, explosions
#define ASCII_S     83    // 'S' -- starbase (chart marker)
#define ASCII_PIPE  124   // '|' -- radar frame
#define ASCII_BLK1  0x11  // light solid block (sprites, asteroids)
#define ASCII_BLK2  0x12
#define ASCII_BLK3  0x13
#define ASCII_BLK4  0x14  // full solid block

// galaxy structure: 8x8 quadrants, each 8x8 sectors
#define GALAXY_QUADS       8
#define SECTORS_PER_QUAD   8
#define QUAD_SIZE   1024
#define QUAD_HALF    512

// 3D space
#define STAR_COUNT        220
#define NEAR_Z             50
#define FAR_Z            1050
#define DEPTH_Z           990
#define FIELD_XY          700
#define FOCAL             300

// engines: gear -1 (slow reverse) .. 8 (fastest sublight)
#define GEAR_MIN           -1
#define GEAR_MAX            8
#define GEAR_SPEED          3    // units per frame per gear step

#define WARP_FRAMES       480    // 8 seconds of acceleration to jump
#define WARP_SPEED         60
#define TURN_ACCEL      0.0018  // angular acceleration (the ship has mass)
#define TURN_DAMP        0.90   // velocity damping per frame
#define TURN_MAX         0.045  // max angular velocity (agile fighter)

// missiles (one fired per X press, alternating cannons)
#define MISSILE_COUNT       6
#define MISSILE_STEP       30    // z advance per frame
#define MISSILE_COST      1.0
#define MISSILE_LATERAL   75    // corner launch offset (x)
#define MISSILE_DROP      38    // corner launch offset (y, down)

// starbases: fixed quadrants; park nearby (any gear) and a repair
// shuttle flies out to you -- repairs apply when it arrives
#define DOCK_RANGE        300
#define DOCK_ZMIN        -150
#define DOCK_ZMAX         500
#define REPAIR_STEPS      130   // shuttle flight frames

// asteroids
#define AST_COUNT           8
#define AST_DAMAGE        18.0

// combat
#define MAX_ENEMIES          8
#define PHASER_COST        1.0
#define WARP_COST         12.0
#define COLLISION_DAMAGE  25.0
#define ENERGY_REGEN      0.03
#define SHIELD_REGEN      0.02
#define MSG_FRAMES        120

// enemy fire
#define EBOLT_COUNT         8
#define EBOLT_SPEED        30
#define EBOLT_DAMAGE      10.0
#define ENEMY_MIN_RANGE   150
#define FLASH_FRAMES        5

// battle damage odds per shield hit
#define CANNON_HIT_ODDS     3
#define ENGINE_HIT_ODDS     4
#define SHIELD_HIT_ODDS     5

// component damage: 4 systems, each with a 9-position gauge
// (3 red / 3 yellow / 3 green, 9 = perfect, 0 = destroyed).
// Hits add damage POINTS; every 3 points steps the gauge down.
// Difficulty sets points per hit: easy 1, medium 2, hard 3.
#define COMP_ENG  0
#define COMP_SHD  1
#define COMP_CMP  2
#define COMP_CAN  3
#define COMP_COUNT 4

// shields & attack computer
#define SHIELD_DRAIN     0.012

// red alert klaxon duration (frames)
#define ALERT_FRAMES      150

// hyperspace navigation: during the 8 s acceleration the pilot must
// keep the ship aligned with the course (marker near the reticle);
// more error than this at jump time = off course, wrong quadrant.
// Drift is a random walk -- noticeable work to counter.
#define NAV_TOLERANCE     0.10
#define NAV_DRIFT       0.0038

// emerging from hyperspace: deceleration from warp speed back down
// to the set engine gear (frames)
#define DECEL_FRAMES      110

// controls are heavier in hyperspace: stronger damping and a lower
// turn rate cap than in normal space
#define WARP_TURN_DAMP    0.80
#define WARP_TURN_MAX     0.020

// explosion debris: block fragments that fly apart and fade
#define DEBRIS_COUNT       24
#define DEBRIS_PER_BURST    6
#define DEBRIS_LIFE_MIN    60    // 1 second at 60 fps
#define DEBRIS_LIFE_MAX   180    // 3 seconds

// Zylon migration: every ~2 minutes the Zylons shift between
// neighbouring quadrants (the starting sector loses its
// protection once the first migration has happened)
#define MIGRATE_FRAMES   7200    // 120 seconds at 60 fps

// ---------------------------------------------------------------------------
//  HUD text (ASCII code lists; plain text in comments)
// ---------------------------------------------------------------------------

int hud_line[64];
int hud_num[16];

int s_quadrant[10]  = { 81, 85, 65, 68, 82, 65, 78, 84, 32, 0 };          // "QUADRANT "
int s_comma[2]      = { 44, 0 };                                          // ","
int s_sector[9]     = { 32, 32, 83, 69, 67, 84, 79, 82, 0 };              // "  SECTOR "
int s_zylons[9]     = { 90, 89, 76, 79, 78, 83, 58, 32, 0 };              // "ZYLONS: "
int s_energy[7]     = { 69, 78, 69, 82, 71, 89, 0 };                      // "ENERGY"
int s_shields[8]    = { 83, 72, 73, 69, 76, 68, 83, 0 };                  // "SHIELDS"
int s_spd[4]        = { 83, 80, 68, 0 };                                  // "SPD"
int s_warp[15]      = { 69, 78, 71, 73, 78, 69, 83, 58, 32, 87, 65, 82, 80, 0 };        // "ENGINES: WARP"
int s_rev[17]       = { 69, 78, 71, 73, 78, 69, 83, 58, 32, 82, 69, 86, 69, 82, 83, 69, 0 }; // "ENGINES: REVERSE"
int s_stop[17]      = { 69, 78, 71, 73, 78, 69, 83, 58, 32, 83, 84, 79, 80, 80, 69, 68, 0 }; // "ENGINES: STOPPED"
int s_cruise[16]    = { 69, 78, 71, 73, 78, 69, 83, 58, 32, 67, 82, 85, 73, 83, 69, 0 };    // "ENGINES: CRUISE"
int s_head[9]       = { 32, 32, 72, 69, 65, 68, 58, 32, 0 };              // "  HEAD: "
int s_n[2] = { 78, 0 };   // "N"
int s_e[2] = { 69, 0 };   // "E"
int s_s[2] = { 83, 0 };   // "S"
int s_w[2] = { 87, 0 };   // "W"
int s_clear[13]     = { 83, 69, 67, 84, 79, 82, 32, 67, 76, 69, 65, 82, 0 };      // "SECTOR CLEAR"
int s_destroyed[15] = { 83, 72, 73, 80, 32, 68, 69, 83, 84, 82, 79, 89, 69, 68, 0 }; // "SHIP DESTROYED"
int s_restart[12]   = { 80, 82, 69, 83, 83, 32, 83, 84, 65, 82, 84, 0 };          // "PRESS START"
int s_chart[15]     = { 71, 65, 76, 65, 67, 84, 73, 67, 32, 67, 72, 65, 82, 84, 0 }; // "GALACTIC CHART"
int s_yclose[9]     = { 89, 58, 32, 67, 76, 79, 83, 69, 0 };                      // "Y: CLOSE"
int s_hdg[6]        = { 72, 68, 71, 58, 32, 0 };                                  // "HDG: "
int s_pit[6]        = { 32, 32, 80, 73, 84, 0 };                                  // "  PIT"
int s_pos[6]        = { 32, 32, 88, 89, 58, 0 };                                  // "  XY:"
int s_dmg_c0[10]    = { 67, 65, 78, 78, 79, 78, 58, 32, 48, 0 };                 // "CANNON: 0"
int s_dmg_c1[10]    = { 67, 65, 78, 78, 79, 78, 58, 32, 49, 0 };                 // "CANNON: 1"
int s_dmg_c2[10]    = { 67, 65, 78, 78, 79, 78, 58, 32, 50, 0 };                 // "CANNON: 2"
int s_dmg_e0[9]     = { 69, 78, 71, 73, 78, 69, 58, 32, 0 };                     // "ENGINE: "
int s_dmg_ok[3]     = { 79, 75, 0 };                                             // "OK"
int s_dmg_bad[5]    = { 68, 65, 77, 33, 0 };                                     // "DAM!"
int s_docked[7]     = { 68, 79, 67, 75, 69, 68, 0 };                             // "DOCKED"
int s_energyrep[19] = { 69, 78, 69, 82, 71, 89, 32, 82, 69, 80, 76, 69, 78, 73, 83, 72, 69, 68, 0 };      // "ENERGY REPLENISHED"
int s_repcomp[17]   = { 82, 69, 80, 65, 73, 82, 83, 32, 67, 79, 77, 80, 76, 69, 84, 69, 0 };              // "REPAIRS COMPLETE"
int s_repair[17]    = { 83, 89, 83, 84, 69, 77, 83, 32, 82, 69, 83, 84, 79, 82, 69, 68, 0 }; // "SYSTEMS RESTORED"
int s_cost[7]       = { 67, 79, 83, 84, 58, 32, 0 };                             // "COST: "
int s_awarp[10]     = { 32, 32, 65, 58, 32, 87, 65, 82, 80, 0 };                 // "  A: WARP"
int s_sh_on[9]      = { 83, 72, 76, 68, 58, 32, 79, 78, 0 };                     // "SHLD: ON"
int s_sh_off[10]    = { 83, 72, 76, 68, 58, 32, 79, 70, 70, 0 };                 // "SHLD: OFF"
int s_ac_on[9]      = { 65, 67, 77, 80, 58, 32, 79, 78, 0 };                     // "ACMP: ON"
int s_ac_off[10]    = { 65, 67, 77, 80, 58, 32, 79, 70, 70, 0 };                 // "ACMP: OFF"
int s_aft[10]       = { 86, 73, 69, 87, 58, 32, 65, 70, 84, 0 };                 // "VIEW: AFT"
int s_fwd[10]       = { 86, 73, 69, 87, 58, 32, 70, 87, 68, 0 };                 // "VIEW: FWD"
int s_near[7]       = { 78, 69, 65, 82, 58, 32, 0 };                             // "NEAR: "
int s_sp[2]         = { 32, 0 };                                                 // " "
int s_zch[2]        = { 90, 0 };                                                 // "Z"
int s_bch[2]        = { 83, 0 };                                                 // "S"
int s_pause[7]      = { 80, 65, 85, 83, 69, 68, 0 };                             // "PAUSED"
int s_ph1[16]       = { 83, 84, 65, 82, 84, 43, 65, 58, 32, 65, 84, 84, 65, 67, 75, 0 }; // "START+A: ATTACK"
int s_ph2[17]       = { 83, 84, 65, 82, 84, 43, 66, 58, 32, 83, 72, 73, 69, 76, 68, 83, 0 }; // "START+B: SHIELDS"
int s_move[16]      = { 90, 89, 76, 79, 78, 83, 32, 83, 72, 73, 70, 84, 73, 78, 71, 0 };  // "ZYLONS SHIFTING"
int s_offcourse[11] = { 79, 70, 70, 32, 67, 79, 85, 82, 83, 69, 0 };                       // "OFF COURSE"
int s_leng[4]       = { 69, 78, 71, 0 };                    // "ENG"
int s_lshd[4]       = { 83, 72, 68, 0 };                    // "SHD"
int s_lcmp[4]       = { 67, 77, 80, 0 };                    // "CMP"
int s_lcan[4]       = { 67, 65, 78, 0 };                    // "CAN"
int s_easy[5]       = { 69, 65, 83, 89, 0 };                // "EASY"
int s_medium[7]     = { 77, 69, 68, 73, 85, 77, 0 };        // "MEDIUM"
int s_hard[5]       = { 72, 65, 82, 68, 0 };                // "HARD"
int s_select[19]    = { 83, 69, 76, 69, 67, 84, 32, 68, 73, 70, 70, 73, 67, 85, 76, 84, 89, 0 }; // "SELECT DIFFICULTY" (18 chars + null = 19)

// 5x7 block font for the title word "STAR RAIDERS" (row-major,
// 1 = draw a block cell). Letters: S T A R I D E.
int pat_S[35] = { 0,1,1,1,1, 1,0,0,0,0, 1,0,0,0,0, 0,1,1,1,0, 0,0,0,0,1, 0,0,0,0,1, 1,1,1,1,0 };
int pat_T[35] = { 1,1,1,1,1, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,0 };
int pat_A[35] = { 0,1,1,1,0, 1,0,0,0,1, 1,0,0,0,1, 1,1,1,1,1, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1 };
int pat_R[35] = { 1,1,1,1,0, 1,0,0,0,1, 1,0,0,0,1, 1,1,1,1,0, 1,0,1,0,0, 1,0,0,1,0, 1,0,0,0,1 };
int pat_I[35] = { 1,1,1,1,1, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,0, 1,1,1,1,1 };
int pat_D[35] = { 1,1,1,1,0, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1, 1,1,1,1,0 };
int pat_E[35] = { 1,1,1,1,1, 1,0,0,0,0, 1,0,0,0,0, 1,1,1,1,0, 1,0,0,0,0, 1,0,0,0,0, 1,1,1,1,1 };
// "STAR RAIDERS": indices into the pattern set (S=0 T=1 A=2 R=3 I=4 D=5 E=6)
int title_word[11] = { 0, 1, 2, 3, 3, 2, 4, 5, 6, 3, 0 };
int s_astfield[10]  = { 65, 83, 84, 69, 82, 79, 73, 68, 83, 0 };                 // "ASTEROIDS"
int s_fullstop[24]  = { 68, 79, 67, 75, 73, 78, 71, 32, 82, 69, 81, 85, 73, 82, 69, 83, 32, 70, 85, 76, 76, 32, 83, 0 }; // "DOCKING REQUIRES FULL "

void hud_append_int( int v )
{
    itoa( v, hud_num, 10 );
    strcat( hud_line, hud_num );
}

// ---------------------------------------------------------------------------
//  Centered / sized glyph drawing. BIOS font hotspots sit at each cell's
//  TOP-LEFT (print_at depends on that), and hotspot coordinates are
//  ABSOLUTE texture positions -- so we never touch hotspots. A 10x20
//  cell drawn at scale (sx, sy) extends (10*sx, 20*sy) right/down from
//  its drawing point, so we draw at (px - 5*sx, py - 10*sy) to center.
// ---------------------------------------------------------------------------

void draw_zoomed_rect( int sx, int sy, float xs, float ys )
{
    set_drawing_scale( xs, ys );
    draw_region_zoomed_at( sx - 5 * xs, sy - 10 * ys );
}

void draw_zoomed_centered( int sx, int sy, float s )
{
    draw_zoomed_rect( sx, sy, s, s );
}

// ---------------------------------------------------------------------------
//  Deterministic RNG (own LCG, independent of the hardware RNG)
// ---------------------------------------------------------------------------

class RNG
{
public:
    int state;

    void seed( int s )
    {
        if (s == 0)
            s = 1;
        state = s;
    }

    int next()
    {
        state = state * 1103515245 + 12345;
        return (state >> 16) & 32767;
    }

    int between( int lo, int hi )
    {
        return lo + (next() % (hi - lo + 1));
    }
};

// ---------------------------------------------------------------------------
//  Star
// ---------------------------------------------------------------------------

class Star
{
public:
    float x;
    float y;
    float z;
    int tint;

    void randomize( RNG *rng )
    {
        x = rng->between( -FIELD_XY, FIELD_XY );
        y = rng->between( -FIELD_XY, FIELD_XY );
        // spread over the FULL depth range, ahead AND behind: stars
        // that pass us keep flying aft for a long time before
        // recycling, so a forward-only initial placement steadily
        // drains the fore view as the field reaches steady state
        z = rng->between( 0 - FAR_Z, FAR_Z );
        if (z > -NEAR_Z && z < NEAR_Z)
            z = NEAR_Z + rng->between( 0, 300 );
        tint = rng->between( 0, 3 );
    }

    // respawn in the FAR band only: recycled stars always fade in
    // from deep space instead of popping in close and large
    void respawn( RNG *rng )
    {
        x = rng->between( -FIELD_XY, FIELD_XY );
        y = rng->between( -FIELD_XY, FIELD_XY );
        z = FAR_Z - rng->between( 0, 300 );
    }
};

// ---------------------------------------------------------------------------
//  Enemy ship
// ---------------------------------------------------------------------------

class Enemy
{
public:
    float x;
    float y;
    float z;
    int alive;
    int kind;      // 0 slow, 1 fast
    int cd;        // frames until it can fire again

    void randomize( RNG *rng )
    {
        x = rng->between( -FIELD_XY, FIELD_XY );
        y = rng->between( -FIELD_XY, FIELD_XY );
        z = rng->between( 400, FAR_Z );
        alive = 1;
        kind = rng->between( 0, 1 );
        cd = rng->between( 40, 140 );
    }
};

// ---------------------------------------------------------------------------
//  Asteroid: stationary in space (streams past with our motion); made
//  of one of the four solid-block glyphs, so 'kind' picks both the
//  texture density and the aspect ratio
// ---------------------------------------------------------------------------

class Asteroid
{
public:
    float x;
    float y;
    float z;
    int kind;      // 0..3 -> blocks 0x11..0x14
    float asp;     // aspect variation

    void respawn( RNG *rng )
    {
        x = rng->between( -FIELD_XY, FIELD_XY );
        y = rng->between( -FIELD_XY, FIELD_XY );
        z = NEAR_Z + 40 + rng->between( 0, DEPTH_Z - 60 );
        kind = rng->between( 0, 3 );
        asp = 0.7 + rng->between( 0, 6 ) * 0.1;
    }
};

// ---------------------------------------------------------------------------
//  Missile: a real 3D projectile. Launched from a wing turret beside
//  the ship (x = +-MISSILE_LATERAL, y = -25), flies down-range,
//  converging gently toward the view axis.
// ---------------------------------------------------------------------------

class Missile
{
public:
    float x;
    float y;
    float z;
    int active;
    int side;      // -1 left cannon, +1 right cannon
};

// ---------------------------------------------------------------------------
//  Enemy bolt
// ---------------------------------------------------------------------------

class EnemyBolt
{
public:
    float x;
    float y;
    float z;
    int active;
};

// ---------------------------------------------------------------------------
//  Debris: one exploding fragment. Lives in world space, flies apart
//  from the explosion origin, fades out over 1-3 seconds.
// ---------------------------------------------------------------------------

class Debris
{
public:
    float x;
    float y;
    float z;
    float vx;
    float vy;
    float vz;
    int t;      // frames of life remaining
    int life;   // total lifetime (for the fade)
    int kind;   // 0-3: which block glyph to draw
};

// ---------------------------------------------------------------------------
//  The game
// ---------------------------------------------------------------------------

class Starfield
{
public:
    Star stars[220];
    Enemy enemies[8];
    Asteroid asteroids[8];
    Missile missiles[6];
    EnemyBolt ebolts[8];
    Debris debris[24];
    RNG rng;

    int qx;
    int qy;
    float shipx;
    float shipy;
    float yaw;
    float pitch;
    float yaw_vel;    // angular velocity (turning inertia)
    float pitch_vel;
    int warp_t;
    int warp_dir;
    int warp_targeted;

    float energy;
    float shields;
    int kills;
    int msg_t;
    int dead;

    int comp_hp[4];     // component health (9 = perfect, 0 = destroyed)
    int comp_pts[4];    // pending damage points (3 points = 1 step)
    int flash_t;

    int gear;           // -1 reverse .. 8 fastest
    int fire_side;      // next cannon to fire (alternating)
    int prev_l;
    int prev_r;
    int prev_start;
    int start_combo;
    int prev_ba;
    int prev_bb;
    int prev_bx;
    int pause_on;
    int computer_on;
    int shields_on;
    int sb_active;
    float sb_x;
    float sb_y;
    float sb_z;
    int docked_t;
    int dock_hint;      // 1 = near base but not stopped

    int rep_state;     // repair shuttle: 0 docked at base, 1 outbound, 2 returning
    int rep_t;          // flight progress (frames)
    int rep_had_dmg;    // choose the completion message

    int ast_active;     // asteroid field in this quadrant
    int ast_count;

    int chart_on;
    int aft_on;       // rear view (B toggles)
    int prev_y;
    int migrate_t;    // Zylon migration clock (frames)
    int move_t;       // "ZYLONS SHIFTING" message timer
    int alert_t;      // red alert klaxon timer
    int offc_t;       // "OFF COURSE" message timer
    int engine_on;    // engine loop currently playing (channel 0)
    int decel_t;      // post-warp deceleration frames remaining
    int target_qx;    // pending chart warp destination
    int target_qy;
    int screen;       // 0 = title / menu, 1 = game
    int title_sel;    // menu cursor: 0 easy, 1 medium, 2 hard
    int diff;         // active difficulty (0/1/2)

    float mk_yaw;     // hyperspace course marker (radians, ship-relative)
    float mk_pitch;
    int galaxy_map[64];
    int cleared[64];
    int ast_map[64];    // asteroid fields per quadrant

    int chart_cx;
    int chart_cy;
    int galaxy_seed;   // per-game random offset (hardware RNG)
    int prev_bl;
    int prev_br;
    int prev_bu;
    int prev_bd;

    // ------------------------------------------------------------------

    int is_starbase_quad( int cqx, int cqy )
    {
        if (cqx == 1 && cqy == 1)
            return 1;
        if (cqx == 6 && cqy == 2)
            return 1;
        if (cqx == 2 && cqy == 6)
            return 1;
        if (cqx == 5 && cqy == 5)
            return 1;
        return 0;
    }

    // deterministic contents of a quadrant we have NOT entered:
    // replicates enter_quadrant's RNG sequence (140 stars x 4 rolls)
    void probe_quadrant( int cqx, int cqy )
    {
        int i;
        rng.seed( (cqy * GALAXY_QUADS + cqx + 1) * 7919 + galaxy_seed );
        i = 0;
        while (i < STAR_COUNT * 4)
        {
            rng.next();
            i = i + 1;
        }
        // enemy count
        galaxy_map[cqy * GALAXY_QUADS + cqx] = 0;
        if (rng.between( 0, 3 ) < 3)
            galaxy_map[cqy * GALAXY_QUADS + cqx] = rng.between( 1, 5 );
        // asteroid field?
        ast_map[cqy * GALAXY_QUADS + cqx] = 0;
        if (rng.between( 0, 2 ) < 1)
            ast_map[cqy * GALAXY_QUADS + cqx] = rng.between( 3, AST_COUNT );
    }

    void enter_quadrant( int nqx, int nqy )
    {
        int i;
        int count;
        int idx;
        qx = nqx;
        qy = nqy;
        idx = qy * GALAXY_QUADS + qx;
        rng.seed( (qy * GALAXY_QUADS + qx + 1) * 7919 + galaxy_seed );
        i = 0;
        while (i < STAR_COUNT)
        {
            stars[i].randomize( &rng );
            i = i + 1;
        }
        count = galaxy_map[idx];
        if (cleared[idx] != 0)
            count = 0;
        i = 0;
        while (i < 8)
        {
            if (i < count)
                enemies[i].randomize( &rng );
            else
                enemies[i].alive = 0;
            i = i + 1;
        }
        // asteroid field (cleared only stops enemies, not rocks)
        ast_count = ast_map[idx];
        ast_active = 0;
        if (ast_count > 0)
        {
            ast_active = 1;
            i = 0;
            while (i < AST_COUNT)
            {
                if (i < ast_count)
                    asteroids[i].respawn( &rng );
                i = i + 1;
            }
        }
        // stray rocks: even quadrants without a proper field can
        // hold one or two lone asteroids (Zylons and rocks coexist)
        if (ast_count == 0)
        {
            if (rng.between( 0, 3 ) < 1)
            {
                ast_count = rng.between( 1, 2 );
                ast_active = 1;
                i = 0;
                while (i < ast_count)
                {
                    asteroids[i].respawn( &rng );
                    i = i + 1;
                }
            }
        }
        i = 0;
        while (i < DEBRIS_COUNT)
        {
            debris[i].t = 0;
            i = i + 1;
        }
        i = 0;
        while (i < EBOLT_COUNT)
        {
            ebolts[i].active = 0;
            i = i + 1;
        }
        i = 0;
        while (i < MISSILE_COUNT)
        {
            missiles[i].active = 0;
            i = i + 1;
        }
        // starbase: dead ahead at a comfortable distance
        if (is_starbase_quad( qx, qy ) != 0)
        {
            sb_active = 1;
            sb_x = rng.between( -150, 150 );
            sb_y = rng.between( -100, 100 );
            sb_z = 700;
        }
        else
            sb_active = 0;
        msg_t = 0;
        // red alert when we arrive somewhere hostile: klaxon in a
        // dedicated channel, stopped by a timer in update()
        if (enemies_alive() > 0)
        {
            play_sound_in_channel( SFX_ALERT, 2 );
            alert_t = ALERT_FRAMES;
        }
    }

    void init( int start_qx, int start_qy )
    {
        int i;
        yaw = 0;
        pitch = 0;
        yaw_vel = 0;
        pitch_vel = 0;
        warp_t = 0;
        warp_dir = 0;
        warp_targeted = 0;
        shipx = QUAD_HALF;
        shipy = QUAD_HALF;
        energy = 100;
        shields = 100;
        kills = 0;
        dead = 0;
        msg_t = 0;
        docked_t = 0;
        dock_hint = 0;
        rep_state = 0;
        rep_t = 0;
        rep_had_dmg = 0;
        // all component gauges perfect
        i = 0;
        while (i < COMP_COUNT)
        {
            comp_hp[i] = 9;
            comp_pts[i] = 0;
            i = i + 1;
        }
        flash_t = 0;
        gear = 2;
        fire_side = -1;
        prev_l = 0;
        prev_r = 0;
        prev_start = 0;
        start_combo = 0;
        prev_ba = 0;
        prev_bb = 0;
        prev_bx = 0;
        pause_on = 0;
        computer_on = 0;
        shields_on = 0;
        chart_on = 0;
        aft_on = 0;
        prev_y = 0;
        migrate_t = 0;
        move_t = 0;
        alert_t = 0;
        offc_t = 0;
        engine_on = 1;
        decel_t = 0;
        target_qx = 0;
        target_qy = 0;
        screen = 1;
        mk_yaw = 0;
        mk_pitch = 0;
        chart_cx = start_qx;
        chart_cy = start_qy;
        // fresh layout every game: salt the per-quadrant seeds with
        // the hardware RNG so nothing is ever in a known place
        galaxy_seed = rand() % 1000000;
        if (galaxy_seed == 0)
            galaxy_seed = 1;

        // engine hum: looped forever in channel 0, volume follows
        // the throttle (see update()). Explicit loop points: the
        // default loop end can be 0, which silences the "loop".
        // The engine sample is 2 s = 88200 samples at 44100 Hz.
        select_sound( SFX_ENGINE );
        set_sound_loop_start( 0 );
        set_sound_loop_end( 88200 );
        set_sound_loop( true );
        play_sound_in_channel( SFX_ENGINE, 0 );

        // missiles fire into their own channel, kept at half volume
        select_channel( 1 );
        set_channel_volume( 0.45 );
        prev_bl = 0;
        prev_br = 0;
        prev_bu = 0;
        prev_bd = 0;
        i = 0;
        while (i < GALAXY_QUADS * GALAXY_QUADS)
        {
            probe_quadrant( i % GALAXY_QUADS, i / GALAXY_QUADS );
            cleared[i] = 0;
            i = i + 1;
        }
        // the starting quadrant is Zylon-free so the player can
        // get their bearings before the fight begins
        galaxy_map[start_qy * GALAXY_QUADS + start_qx] = 0;
        enter_quadrant( start_qx, start_qy );
    }

    // current cruise speed from the engine gear
    float engine_speed()
    {
        if (gear < 0)
            return gear * GEAR_SPEED;      // -3 in reverse
        return gear * GEAR_SPEED;          // 0 stopped, up to 24
    }

    // compass heading (0=N 1=E 2=S 3=W) quantized from the yaw
    int heading()
    {
        int h;
        float a;
        a = yaw / 1.5707963;
        h = a;
        if (a - h >= 0.5)
            h = h + 1;
        h = h % 4;
        if (h < 0)
            h = h + 4;
        return h;
    }

    int nav_heading_deg()
    {
        int d;
        d = yaw * 57.29578;
        d = d % 360;
        if (d < 0)
            d = d + 360;
        return d;
    }

    int nav_pitch_deg()
    {
        return pitch * 57.29578;
    }

    int enemies_alive()
    {
        int i;
        int n;
        n = 0;
        i = 0;
        while (i < 8)
        {
            if (enemies[i].alive != 0)
                n = n + 1;
            i = i + 1;
        }
        return n;
    }

    // phaser cone half-width factor (attack computer widens it)
    float cone_factor()
    {
        if (computer_on != 0)
            return 0.22;
        return 0.16;
    }

    // ------------------------------------------------------------------

    // engine hum: silent at rest, starts with motion, volume follows
    // the throttle; full blast during warp
    void update_engine_sound()
    {
        float vol;
        if (gear == 0 && warp_t == 0 && decel_t == 0)
        {
            if (engine_on != 0)
            {
                stop_channel( 0 );
                engine_on = 0;
            }
            return;
        }
        if (engine_on == 0)
        {
            play_sound_in_channel( SFX_ENGINE, 0 );
            engine_on = 1;
        }
        vol = 0.12 + gear_frac() * 0.7;
        if (warp_t > 0)
            vol = 1.0;
        select_channel( 0 );
        set_channel_volume( vol );
    }

    // begin a hyperspace run: reset the course marker near center
    // (small random error), clear the sector (we are accelerating
    // past everything in it) and start the jump sound in channel 3
    void nav_start_warp()
    {
        int i;
        mk_yaw = rng.between( -6, 6 ) * 0.01;
        mk_pitch = rng.between( -6, 6 ) * 0.01;
        // anything in this sector falls behind as we jump: enemies,
        // their bolts, our own in-flight missiles
        i = 0;
        while (i < 8)
        {
            enemies[i].alive = 0;
            i = i + 1;
        }
        i = 0;
        while (i < EBOLT_COUNT)
        {
            ebolts[i].active = 0;
            i = i + 1;
        }
        i = 0;
        while (i < MISSILE_COUNT)
        {
            missiles[i].active = 0;
            i = i + 1;
        }
        play_sound_in_channel( SFX_HYPERSPACE, 3 );
    }

    // alignment check at jump time
    int nav_misaligned()
    {
        if (mk_yaw > NAV_TOLERANCE)
            return 1;
        if (mk_yaw < 0 - NAV_TOLERANCE)
            return 1;
        if (mk_pitch > NAV_TOLERANCE)
            return 1;
        if (mk_pitch < 0 - NAV_TOLERANCE)
            return 1;
        return 0;
    }

    // misaligned jump: we surface one quadrant off, randomly
    void warp_offcourse()
    {
        int dir;
        int nqx;
        int nqy;
        dir = rng.between( 0, 3 );
        nqx = qx;
        nqy = qy;
        if (dir == 0)
            nqy = qy - 1;
        else if (dir == 1)
            nqx = qx + 1;
        else if (dir == 2)
            nqy = qy + 1;
        else
            nqx = qx - 1;
        while (nqx < 0)
            nqx = nqx + GALAXY_QUADS;
        while (nqx >= GALAXY_QUADS)
            nqx = nqx - GALAXY_QUADS;
        while (nqy < 0)
            nqy = nqy + GALAXY_QUADS;
        while (nqy >= GALAXY_QUADS)
            nqy = nqy - GALAXY_QUADS;
        enter_quadrant( nqx, nqy );
        offc_t = MSG_FRAMES;
    }

    // ------------------------------------------------------------------
    //  title screen / difficulty menu
    // ------------------------------------------------------------------

    void init_title()
    {
        int i;
        screen = 0;
        title_sel = 1;
        prev_ba = 0;
        prev_bu = 0;
        prev_bd = 0;
        prev_start = 0;
        rng.seed( rand() % 1000000 );
        i = 0;
        while (i < STAR_COUNT)
        {
            stars[i].randomize( &rng );
            i = i + 1;
        }
        // title music: looped in channel 4
        stop_channel( 4 );
        select_sound( SFX_TITLE );
        set_sound_loop_start( 0 );
        set_sound_loop_end( 352800 );
        set_sound_loop( true );
        play_sound_in_channel( SFX_TITLE, 4 );
    }

    void start_game()
    {
        diff = title_sel;
        // swap the title track for the gameplay track
        stop_channel( 4 );
        select_sound( SFX_GAME );
        set_sound_loop_start( 0 );
        set_sound_loop_end( 529200 );
        set_sound_loop( true );
        play_sound_in_channel( SFX_GAME, 4 );
        init( 3, 4 );
    }

    void update_title()
    {
        int i;
        // slow starfield drift
        i = 0;
        while (i < STAR_COUNT)
        {
            stars[i].z = stars[i].z - 9;
            if (stars[i].z < 0 - FAR_Z - 150)
                stars[i].respawn( &rng );
            i = i + 1;
        }
        // menu navigation
        if (gamepad_up() > 0 && prev_bu <= 0)
            title_sel = title_sel - 1;
        if (gamepad_down() > 0 && prev_bd <= 0)
            title_sel = title_sel + 1;
        if (title_sel < 0)
            title_sel = 2;
        if (title_sel > 2)
            title_sel = 0;
        prev_bu = gamepad_up();
        prev_bd = gamepad_down();
        if ((gamepad_button_a() > 0 && prev_ba <= 0) ||
            (gamepad_button_start() > 0 && prev_start <= 0))
        {
            play_sound( SFX_BEEP );
            start_game();
            return;
        }
        prev_ba = gamepad_button_a();
        prev_start = gamepad_button_start();
    }

    // draw one title letter from its 5x7 pattern; shade picks the
    // block density (0 = full solid 0x14 ... 3 = lightest 0x11)
    void draw_title_letter( int letter, int x0, int y0, int shade, int shadow )
    {
        int* pat;
        int r;
        int c;
        pat = pat_S;
        if (letter == 1)
            pat = pat_T;
        else if (letter == 2)
            pat = pat_A;
        else if (letter == 3)
            pat = pat_R;
        else if (letter == 4)
            pat = pat_I;
        else if (letter == 5)
            pat = pat_D;
        else if (letter == 6)
            pat = pat_E;
        r = 0;
        while (r < 7)
        {
            c = 0;
            while (c < 5)
            {
                if (pat[r * 5 + c] != 0)
                {
                    if (shadow != 0)
                    {
                        select_region( ASCII_BLK4 );
                        set_multiply_color( make_color_rgb( 25, 25, 35 ) );
                    }
                    else
                    {
                        select_region( ASCII_BLK4 - shade );
                        set_multiply_color( color_white );
                    }
                    set_drawing_scale( 0.8, 0.4 );
                    draw_region_zoomed_at( x0 + c * 8, y0 + r * 8 );
                }
                c = c + 1;
            }
            r = r + 1;
        }
    }

    void draw_title()
    {
        int i;
        int lx;
        float d;
        float sx;
        float sy;
        set_blending_mode( blending_alpha );
        set_multiply_color( color_white );
        clear_screen( make_color_rgb( 2, 4, 12 ) );

        // starfield
        select_texture( -1 );
        select_region( ASCII_DOT );
        set_blending_mode( blending_add );
        i = 0;
        while (i < STAR_COUNT)
        {
            d = stars[i].z;
            if (d >= NEAR_Z)
            {
                int b;
                float s;
                sx = CENTER_X + stars[i].x / d * FOCAL;
                sy = CENTER_Y - stars[i].y / d * FOCAL;
                if (sx > -20 && sx < 660 && sy > -20 && sy < 380)
                {
                    b = 255 - d * 0.19;
                    if (b < 60)
                        b = 60;
                    s = 420 / d;
                    if (s < 0.5)
                        s = 0.5;
                    if (s > 3.5)
                        s = 3.5;
                    set_multiply_color( make_color_rgb( b, b, b ) );
                    draw_zoomed_centered( sx, sy, s );
                }
            }
            i = i + 1;
        }
        set_blending_mode( blending_alpha );

        // "STAR RAIDERS" -- 11 letters, progressively lighter
        // blocks (0x14 -> 0x11), with a drop shadow
        lx = 52;
        i = 0;
        while (i < 11)
        {
            draw_title_letter( title_word[i], lx + 6, 80 + 6, 0, 1 );
            i = i + 1;
            lx = lx + 48;
            if (i == 4)
                lx = lx + 16;
        }
        lx = 52;
        i = 0;
        while (i < 11)
        {
            draw_title_letter( title_word[i], lx, 80, i / 3, 0 );
            i = i + 1;
            lx = lx + 48;
            if (i == 4)
                lx = lx + 16;
        }

        // difficulty menu
        set_multiply_color( color_white );
        print_at( 228, 210, s_select );
        if (title_sel == 0)
            set_multiply_color( make_color_rgb( 0, 230, 130 ) );
        else
            set_multiply_color( make_color_rgb( 130, 130, 150 ) );
        print_at( 292, 250, s_easy );
        if (title_sel == 1)
            set_multiply_color( make_color_rgb( 0, 230, 130 ) );
        else
            set_multiply_color( make_color_rgb( 130, 130, 150 ) );
        print_at( 286, 280, s_medium );
        if (title_sel == 2)
            set_multiply_color( make_color_rgb( 0, 230, 130 ) );
        else
            set_multiply_color( make_color_rgb( 130, 130, 150 ) );
        print_at( 292, 310, s_hard );
        // blinking selection marker
        if (get_frame_counter() % 40 < 25)
        {
            select_region( ASCII_PLUS );
            set_multiply_color( make_color_rgb( 0, 230, 130 ) );
            draw_zoomed_centered( 262, 260 + title_sel * 30, 0.8 );
        }
        set_multiply_color( color_white );
    }

    void update()
    {
        float speed;
        int i;
        int d;
        float drift;

        if (screen == 0)
        {
            update_title();
            return;
        }

        if (dead != 0)
        {
            if (gamepad_button_start() > 0)
                init( 3, 4 );
            return;
        }

        // Start handling: Start+A toggles the attack computer,
        // Start+B toggles shields, Start alone pauses
        if (gamepad_button_start() > 0)
        {
            if (prev_start <= 0)
                start_combo = 0;
            if (gamepad_button_a() > 0 && prev_ba <= 0)
            {
                // a destroyed computer cannot be switched back on
                if (comp_hp[COMP_CMP] > 0)
                    computer_on = 1 - computer_on;
                start_combo = 1;
                play_sound( SFX_BEEP );
            }
            if (gamepad_button_b() > 0 && prev_bb <= 0)
            {
                shields_on = 1 - shields_on;
                start_combo = 1;
                play_sound( SFX_BEEP );
            }
            prev_ba = gamepad_button_a();
            prev_bb = gamepad_button_b();
        }
        else
        {
            if (prev_start > 0 && start_combo == 0)
                pause_on = 1 - pause_on;
        }
        prev_start = gamepad_button_start();

        if (pause_on != 0)
            return;

        // Y toggles the galactic chart; opening it puts the cursor
        // on our current quadrant
        if (gamepad_button_y() > 0 && prev_y <= 0)
        {
            chart_on = 1 - chart_on;
            if (chart_on != 0)
            {
                chart_cx = qx;
                chart_cy = qy;
            }
        }
        prev_y = gamepad_button_y();

        // B alone toggles the aft (rear) view; the Start+B shield
        // combo owns B while Start is held
        if (gamepad_button_start() <= 0 && chart_on == 0)
        {
            if (gamepad_button_b() > 0 && prev_bb <= 0)
                aft_on = 1 - aft_on;
            prev_bb = gamepad_button_b();
        }

        if (chart_on != 0)
        {
            update_chart();
            return;
        }

        // view rotation with inertia: the D-pad accelerates angular
        // velocity, damping bleeds it off -- the ship has mass, so
        // turns start and stop with a hint of lag
        if (gamepad_left() > 0)
            yaw_vel = yaw_vel - TURN_ACCEL;
        if (gamepad_right() > 0)
            yaw_vel = yaw_vel + TURN_ACCEL;
        if (gamepad_up() > 0)
            pitch_vel = pitch_vel - TURN_ACCEL;
        if (gamepad_down() > 0)
            pitch_vel = pitch_vel + TURN_ACCEL;
        // hyperspace feels heavier: stronger damping and a lower
        // rate cap than open-space handling
        if (warp_t != 0)
        {
            yaw_vel = yaw_vel * WARP_TURN_DAMP;
            pitch_vel = pitch_vel * WARP_TURN_DAMP;
            if (yaw_vel > WARP_TURN_MAX)
                yaw_vel = WARP_TURN_MAX;
            if (yaw_vel < 0 - WARP_TURN_MAX)
                yaw_vel = 0 - WARP_TURN_MAX;
            if (pitch_vel > WARP_TURN_MAX)
                pitch_vel = WARP_TURN_MAX;
            if (pitch_vel < 0 - WARP_TURN_MAX)
                pitch_vel = 0 - WARP_TURN_MAX;
        }
        else
        {
            yaw_vel = yaw_vel * TURN_DAMP;
            pitch_vel = pitch_vel * TURN_DAMP;
            if (yaw_vel > TURN_MAX)
                yaw_vel = TURN_MAX;
            if (yaw_vel < 0 - TURN_MAX)
                yaw_vel = 0 - TURN_MAX;
            if (pitch_vel > TURN_MAX)
                pitch_vel = TURN_MAX;
            if (pitch_vel < 0 - TURN_MAX)
                pitch_vel = 0 - TURN_MAX;
        }
        if (yaw_vel > 0.0005 || yaw_vel < -0.0005)
        {
            yaw = yaw + yaw_vel;
            rotate_yaw( yaw_vel );
        }
        else
            yaw_vel = 0;
        if (pitch_vel > 0.0005 || pitch_vel < -0.0005)
        {
            pitch = pitch + pitch_vel;
            rotate_pitch( pitch_vel );
        }
        else
            pitch_vel = 0;
        // steering also moves the hyperspace course marker (it is
        // fixed in space; we rotate around it). Both axes flipped,
        // as if trimming directly against the course.
        mk_yaw = mk_yaw + yaw_vel;
        mk_pitch = mk_pitch - pitch_vel;
        if (pitch > 0.61)
            pitch = 0.61;
        if (pitch < -0.61)
            pitch = -0.61;

        // engine throttle: L = down, R = up
        d = gamepad_button_l();
        if (d > 0 && prev_l <= 0 && gear > GEAR_MIN)
            gear = gear - 1;
        prev_l = d;
        d = gamepad_button_r();
        if (d > 0 && prev_r <= 0 && gear < GEAR_MAX)
            gear = gear + 1;
        prev_r = d;

        speed = engine_speed();
        // engine damage: yellow band = half speed, red = dead stick
        if (comp_hp[COMP_ENG] <= 0)
            speed = 0;
        else if (comp_hp[COMP_ENG] < 7)
            speed = speed * 0.5;
        // emerging from hyperspace: still hauling faster than the
        // engines can push; bleed down to the set gear smoothly
        if (decel_t > 0)
        {
            float f;
            decel_t = decel_t - 1;
            f = decel_t;
            f = f / DECEL_FRAMES;
            speed = speed + ( WARP_SPEED - speed ) * f;
        }

        // warp on A (not while Start is held: that is a combo)
        if (warp_t == 0)
        {
            if (gamepad_button_a() > 0 && gamepad_button_start() <= 0 &&
                energy >= WARP_COST)
            {
                energy = energy - WARP_COST;
                warp_t = 1;
                warp_dir = heading();
                nav_start_warp();
            }
        }
        else
        {
            speed = WARP_SPEED;
            warp_t = warp_t + 1;
            // the course drifts during acceleration: keep the
            // marker on the reticle to stay aligned. Harder
            // difficulty = a much jumpier course.
            drift = NAV_DRIFT;
            if (diff == 1)
                drift = NAV_DRIFT * 1.7;
            else if (diff == 2)
                drift = NAV_DRIFT * 2.4;
            mk_yaw = mk_yaw + rng.between( 0 - 1, 1 ) * drift;
            mk_pitch = mk_pitch + rng.between( 0 - 1, 1 ) * drift;
            if (mk_yaw > 0.35)
                mk_yaw = 0.35;
            if (mk_yaw < -0.35)
                mk_yaw = -0.35;
            if (mk_pitch > 0.35)
                mk_pitch = 0.35;
            if (mk_pitch < -0.35)
                mk_pitch = -0.35;
            if (warp_t > WARP_FRAMES)
            {
                int offc;
                warp_t = 0;
                stop_channel( 3 );
                offc = nav_misaligned();
                if (warp_targeted != 0)
                {
                    warp_targeted = 0;
                    shipx = QUAD_HALF;
                    shipy = QUAD_HALF;
                    if (offc != 0)
                        warp_offcourse();
                    else
                        enter_quadrant( target_qx, target_qy );
                }
                else
                {
                    if (offc != 0)
                        warp_offcourse();
                    else
                        arrive();
                }
                // emerge fast: decelerate from warp speed back down
                // to the previously set engine gear
                decel_t = DECEL_FRAMES;
            }
        }

        // move through quadrant space along our facing
        shipx = shipx + sin( yaw ) * speed * 0.5;
        shipy = shipy - cos( yaw ) * speed * 0.5;
        while (shipx < 0)
            shipx = shipx + QUAD_SIZE;
        while (shipx >= QUAD_SIZE)
            shipx = shipx - QUAD_SIZE;
        while (shipy < 0)
            shipy = shipy + QUAD_SIZE;
        while (shipy >= QUAD_SIZE)
            shipy = shipy - QUAD_SIZE;

        // stars stream past (both directions: reverse pushes them away).
        // They recycle only once FAR behind the ship: the aft view
        // sees the band -FAR_Z..-NEAR_Z, so recycling at -NEAR_Z
        // would empty the rear view completely.
        i = 0;
        while (i < STAR_COUNT)
        {
            stars[i].z = stars[i].z - speed;
            if (stars[i].z < 0 - FAR_Z - 150)
                stars[i].respawn( &rng );
            if (stars[i].z > FAR_Z + 150)
                stars[i].z = stars[i].z - DEPTH_Z;
            i = i + 1;
        }

        // asteroids: stationary, they stream past and recycle while
        // we are inside the field
        if (ast_active != 0)
        {
            i = 0;
            while (i < ast_count)
            {
                asteroids[i].z = asteroids[i].z - speed;
                if (asteroids[i].z < 0 - FAR_Z - 150)
                    asteroids[i].respawn( &rng );
                if (asteroids[i].z > FAR_Z + 150)
                    asteroids[i].z = asteroids[i].z - DEPTH_Z;
                // collision with the player
                if (asteroids[i].z < 80 && asteroids[i].z > 0 &&
                    asteroids[i].x > -60 && asteroids[i].x < 60 &&
                    asteroids[i].y > -60 && asteroids[i].y < 60)
                {
                    take_hit( AST_DAMAGE );
                    asteroids[i].respawn( &rng );
                }
                i = i + 1;
            }
        }

        // starbase: stationary; park nearby and a repair shuttle
        // comes out to us
        if (sb_active != 0)
        {
            sb_z = sb_z - speed;
            if (sb_z < DOCK_ZMAX && sb_z > DOCK_ZMIN &&
                sb_x > -DOCK_RANGE && sb_x < DOCK_RANGE &&
                sb_y > -DOCK_RANGE && sb_y < DOCK_RANGE)
            {
                if (gear == 0)
                {
                    dock_hint = 0;
                    if (rep_state == 0)
                    {
                        rep_state = 1;
                        rep_t = 0;
                        rep_had_dmg = 0;
                        if (comp_hp[COMP_ENG] < 9 || comp_hp[COMP_SHD] < 9 ||
                            comp_hp[COMP_CMP] < 9 || comp_hp[COMP_CAN] < 9)
                            rep_had_dmg = 1;
                    }
                }
                else
                    dock_hint = 1;
            }
            else
                dock_hint = 0;
        }

        // repair shuttle: flies out, services us, then RETURNS to the
        // starbase and docks before another can be sent. Departing
        // mid-flight sends it straight home.
        if (rep_state == 1)
        {
            if (gear != 0 || sb_active == 0)
            {
                rep_state = 2;
            }
            else
            {
                rep_t = rep_t + 1;
                if (rep_t >= REPAIR_STEPS)
                {
                    dock();
                    rep_state = 2;
                }
            }
        }
        else if (rep_state == 2)
        {
            rep_t = rep_t - 1;
            if (rep_t <= 0)
            {
                rep_t = 0;
                rep_state = 0;
            }
        }

        // missiles: fired one per X press, alternating cannons
        if (gamepad_button_x() > 0 && prev_bx <= 0)
            fire_missile();
        prev_bx = gamepad_button_x();

        update_missiles();

        if (warp_t == 0)
        {
            update_enemies();
            update_ebolts();
        }
        else
        {
            i = 0;
            while (i < EBOLT_COUNT)
            {
                ebolts[i].active = 0;
                i = i + 1;
            }
        }

        if (flash_t > 0)
            flash_t = flash_t - 1;

        // explosion debris: fly apart, drift back with our motion, fade
        i = 0;
        while (i < DEBRIS_COUNT)
        {
            if (debris[i].t > 0)
            {
                debris[i].x = debris[i].x + debris[i].vx;
                debris[i].y = debris[i].y + debris[i].vy;
                debris[i].z = debris[i].z + debris[i].vz - speed;
                debris[i].t = debris[i].t - 1;
            }
            i = i + 1;
        }

        // regeneration and drains
        energy = energy + ENERGY_REGEN;
        if (energy > 100)
            energy = 100;
        if (shields_on != 0)
        {
            energy = energy - SHIELD_DRAIN;
            if (energy <= 0)
            {
                energy = 0;
                shields_on = 0;
            }
        }
        shields = shields + SHIELD_REGEN;
        if (shields > 100)
            shields = 100;

        if (msg_t > 0)
            msg_t = msg_t - 1;
        if (move_t > 0)
            move_t = move_t - 1;
        if (offc_t > 0)
            offc_t = offc_t - 1;
        // klaxon cutoff: never let the alert outlive its welcome
        if (alert_t > 0)
        {
            alert_t = alert_t - 1;
            if (alert_t == 0)
                stop_channel( 2 );
        }
        if (docked_t > 0)
            docked_t = docked_t - 1;

        update_engine_sound();

        // Zylon migration clock: now and then the Zylon fleet
        // redistributes itself across neighbouring quadrants
        migrate_t = migrate_t + 1;
        if (migrate_t >= MIGRATE_FRAMES)
        {
            migrate_t = 0;
            zylon_migration();
        }
    }

    // ------------------------------------------------------------------
    //  missiles
    // ------------------------------------------------------------------

    void fire_missile()
    {
        int i;
        int side;
        if (comp_hp[COMP_CAN] < 2 || energy < MISSILE_COST)
            return;
        // pick the cannon: with both, alternate; with one, right only
        if (comp_hp[COMP_CAN] >= 7)
        {
            side = fire_side;
            fire_side = 0 - fire_side;
        }
        else
            side = 1;
        // find a free missile slot
        i = 0;
        while (i < MISSILE_COUNT)
        {
            if (missiles[i].active == 0)
            {
                missiles[i].x = side * MISSILE_LATERAL;
                missiles[i].y = 0 - MISSILE_DROP;
                missiles[i].z = NEAR_Z + 30;
                missiles[i].side = side;
                missiles[i].active = 1;
                energy = energy - MISSILE_COST;
                play_sound_in_channel( SFX_MISSILE, 1 );
                return;
            }
            i = i + 1;
        }
    }

    void update_missiles()
    {
        int i;
        i = 0;
        while (i < MISSILE_COUNT)
        {
            if (missiles[i].active != 0)
            {
                missiles[i].z = missiles[i].z + MISSILE_STEP;
                // gentle convergence toward the view axis
                missiles[i].x = missiles[i].x * 0.93;
                missiles[i].y = missiles[i].y * 0.93;
                if (missiles[i].z > FAR_Z)
                    missiles[i].active = 0;
                else
                {
                    missile_hit_enemies( i );
                    if (missiles[i].active != 0 && ast_active != 0)
                        missile_hit_asteroids( i );
                }
            }
            i = i + 1;
        }
    }

    void missile_hit_enemies( int m )
    {
        int i;
        float limf;
        limf = cone_factor();
        i = 0;
        while (i < 8)
        {
            if (enemies[i].alive != 0)
            {
                float dz;
                dz = missiles[m].z - enemies[i].z;
                if (dz < MISSILE_STEP && dz > 0 - MISSILE_STEP)
                {
                    float lim;
                    lim = enemies[i].z * limf;
                    if (missiles[m].x > enemies[i].x - lim &&
                        missiles[m].x < enemies[i].x + lim &&
                        missiles[m].y > enemies[i].y - lim &&
                        missiles[m].y < enemies[i].y + lim)
                    {
                        add_explosion( enemies[i].x, enemies[i].y, enemies[i].z );
                        enemies[i].alive = 0;
                        missiles[m].active = 0;
                        kills = kills + 1;
                        if (enemies_alive() == 0)
                        {
                            cleared[qy * GALAXY_QUADS + qx] = 1;
                            galaxy_map[qy * GALAXY_QUADS + qx] = 0;
                            msg_t = MSG_FRAMES;
                        }
                        return;
                    }
                }
            }
            i = i + 1;
        }
    }

    void missile_hit_asteroids( int m )
    {
        int i;
        i = 0;
        while (i < ast_count)
        {
            float dz;
            dz = missiles[m].z - asteroids[i].z;
            if (dz < MISSILE_STEP && dz > 0 - MISSILE_STEP)
            {
                if (missiles[m].x > asteroids[i].x - 55 &&
                    missiles[m].x < asteroids[i].x + 55 &&
                    missiles[m].y > asteroids[i].y - 55 &&
                    missiles[m].y < asteroids[i].y + 55)
                {
                    add_explosion( asteroids[i].x, asteroids[i].y, asteroids[i].z );
                    asteroids[i].respawn( &rng );
                    missiles[m].active = 0;
                    return;
                }
            }
            i = i + 1;
        }
    }

    void add_explosion( float ex, float ey, float ez )
    {
        int i;
        int n;
        i = 0;
        n = 0;
        while (i < DEBRIS_COUNT && n < DEBRIS_PER_BURST)
        {
            if (debris[i].t == 0)
            {
                float a;
                float spd;
                debris[i].x = ex;
                debris[i].y = ey;
                debris[i].z = ez;
                // random outward direction and speed
                a = rng.between( 0, 628 ) * 0.01;
                spd = rng.between( 12, 40 ) * 0.1;
                debris[i].vx = cos( a ) * spd;
                debris[i].vy = sin( a ) * spd;
                debris[i].vz = rng.between( -25, 25 ) * 0.1;
                debris[i].life = rng.between( DEBRIS_LIFE_MIN, DEBRIS_LIFE_MAX );
                debris[i].t = debris[i].life;
                debris[i].kind = rng.between( 0, 3 );
                n = n + 1;
                play_sound( SFX_EXPLOSION );
            }
            i = i + 1;
        }
    }

    // ------------------------------------------------------------------
    //  enemy fire
    // ------------------------------------------------------------------

    void enemy_fire( int i )
    {
        int j;
        j = 0;
        while (j < EBOLT_COUNT)
        {
            if (ebolts[j].active == 0)
            {
                ebolts[j].x = enemies[i].x;
                ebolts[j].y = enemies[i].y;
                ebolts[j].z = enemies[i].z;
                ebolts[j].active = 1;
                return;
            }
            j = j + 1;
        }
    }

    void update_ebolts()
    {
        int i;
        i = 0;
        while (i < EBOLT_COUNT)
        {
            if (ebolts[i].active != 0)
            {
                float len;
                float nx;
                float ny;
                float nz;
                nx = 0 - ebolts[i].x;
                ny = 0 - ebolts[i].y;
                nz = 0 - ebolts[i].z;
                len = sqrt( nx * nx + ny * ny + nz * nz );
                if (len < EBOLT_SPEED + 40)
                {
                    ebolts[i].active = 0;
                    take_hit( EBOLT_DAMAGE );
                }
                else
                {
                    ebolts[i].x = ebolts[i].x + nx / len * EBOLT_SPEED;
                    ebolts[i].y = ebolts[i].y + ny / len * EBOLT_SPEED;
                    ebolts[i].z = ebolts[i].z + nz / len * EBOLT_SPEED;
                    if (ebolts[i].z < NEAR_Z - 60)
                        ebolts[i].active = 0;
                }
            }
            i = i + 1;
        }
    }

    void take_hit( float dmg )
    {
        flash_t = FLASH_FRAMES;
        if (shields_on == 0)
        {
            // hit with shields down: game over, boom
            dead = 1;
            play_sound( SFX_EXPLOSION );
            add_explosion( 0, 0, 350 );
            return;
        }
        shields = shields - dmg;
        if (shields <= 0)
        {
            shields = 0;
            dead = 1;
            play_sound( SFX_EXPLOSION );
            add_explosion( 0, 0, 350 );
            return;
        }
        apply_component_damage();
    }

    // a shield hit strains a random system: damage points pile up
    // (easy 1 / medium 2 / hard 3 per hit) and every 3 points steps
    // that system's gauge down by one position
    void apply_component_damage()
    {
        int c;
        int pts;
        c = rng.between( 0, COMP_COUNT - 1 );
        pts = diff + 1;
        comp_pts[c] = comp_pts[c] + pts;
        while (comp_pts[c] >= 3)
        {
            comp_pts[c] = comp_pts[c] - 3;
            if (comp_hp[c] > 0)
                comp_hp[c] = comp_hp[c] - 1;
        }
        // destroyed systems have consequences
        if (comp_hp[COMP_SHD] <= 0)
            shields_on = 0;
        if (comp_hp[COMP_CMP] <= 0)
            computer_on = 0;
    }

    void update_enemies()
    {
        int i;
        int cd;
        i = 0;
        while (i < 8)
        {
            if (enemies[i].alive != 0)
            {
                float ddx;
                float ddy;
                float ddz;
                float len;
                float spd;
                ddx = 0 - enemies[i].x;
                ddy = 0 - enemies[i].y;
                ddz = 0 - enemies[i].z;
                len = sqrt( ddx * ddx + ddy * ddy + ddz * ddz );
                spd = 0.8;
                if (enemies[i].kind == 1)
                    spd = 1.5;
                if (len > 1)
                {
                    enemies[i].x = enemies[i].x + ddx / len * spd;
                    enemies[i].y = enemies[i].y + ddy / len * spd;
                    enemies[i].z = enemies[i].z + ddz / len * spd;
                }
                // collision: close in all three axes
                if (enemies[i].z < 90 && enemies[i].z > -90 &&
                    enemies[i].x > -70 && enemies[i].x < 70 &&
                    enemies[i].y > -70 && enemies[i].y < 70)
                {
                    enemies[i].alive = 0;
                    take_hit( COLLISION_DAMAGE );
                }
                // return fire: enemies roughly on screen shoot at us
                if (enemies[i].cd > 0)
                    enemies[i].cd = enemies[i].cd - 1;
                if (enemies[i].cd == 0 && enemies[i].z > ENEMY_MIN_RANGE)
                {
                    float lim;
                    lim = enemies[i].z * 0.35;
                    if (enemies[i].x > -lim && enemies[i].x < lim &&
                        enemies[i].y > -lim && enemies[i].y < lim)
                    {
                        enemy_fire( i );
                        // harder difficulty = faster return fire
                        if (enemies[i].kind == 1)
                            cd = rng.between( 50, 100 );
                        else
                            cd = rng.between( 80, 160 );
                        if (diff == 0)
                            cd = cd * 1.6;
                        else if (diff == 2)
                            cd = cd * 0.65;
                        enemies[i].cd = cd;
                    }
                }
            }
            i = i + 1;
        }
    }

    void arrive()
    {
        int nqx;
        int nqy;
        nqx = qx;
        nqy = qy;
        if (warp_dir == 0)
        {
            nqy = qy - 1;
            shipy = QUAD_SIZE - 64;
        }
        else if (warp_dir == 1)
        {
            nqx = qx + 1;
            shipx = 64;
        }
        else if (warp_dir == 2)
        {
            nqy = qy + 1;
            shipy = 64;
        }
        else
        {
            nqx = qx - 1;
            shipx = QUAD_SIZE - 64;
        }
        while (nqx < 0)
            nqx = nqx + GALAXY_QUADS;
        while (nqx >= GALAXY_QUADS)
            nqx = nqx - GALAXY_QUADS;
        while (nqy < 0)
            nqy = nqy + GALAXY_QUADS;
        while (nqy >= GALAXY_QUADS)
            nqy = nqy - GALAXY_QUADS;
        enter_quadrant( nqx, nqy );
    }

    // ------------------------------------------------------------------
    //  rotation of the world around the camera
    // ------------------------------------------------------------------

    void rotate_yaw( float d )
    {
        float c;
        float s;
        float nx;
        float nz;
        int i;
        c = cos( d );
        s = sin( d );
        i = 0;
        while (i < STAR_COUNT)
        {
            nx = stars[i].x * c - stars[i].z * s;
            nz = stars[i].x * s + stars[i].z * c;
            stars[i].x = nx;
            stars[i].z = nz;
            i = i + 1;
        }
        i = 0;
        while (i < 8)
        {
            if (enemies[i].alive != 0)
            {
                nx = enemies[i].x * c - enemies[i].z * s;
                nz = enemies[i].x * s + enemies[i].z * c;
                enemies[i].x = nx;
                enemies[i].z = nz;
            }
            i = i + 1;
        }
        if (ast_active != 0)
        {
            i = 0;
            while (i < ast_count)
            {
                nx = asteroids[i].x * c - asteroids[i].z * s;
                nz = asteroids[i].x * s + asteroids[i].z * c;
                asteroids[i].x = nx;
                asteroids[i].z = nz;
                i = i + 1;
            }
        }
        if (sb_active != 0)
        {
            nx = sb_x * c - sb_z * s;
            nz = sb_x * s + sb_z * c;
            sb_x = nx;
            sb_z = nz;
        }
        i = 0;
        while (i < DEBRIS_COUNT)
        {
            if (debris[i].t > 0)
            {
                nx = debris[i].x * c - debris[i].z * s;
                nz = debris[i].x * s + debris[i].z * c;
                debris[i].x = nx;
                debris[i].z = nz;
            }
            i = i + 1;
        }
    }

    void rotate_pitch( float d )
    {
        float c;
        float s;
        float ny;
        float nz;
        int i;
        c = cos( d );
        s = sin( d );
        i = 0;
        while (i < STAR_COUNT)
        {
            ny = stars[i].y * c - stars[i].z * s;
            nz = stars[i].y * s + stars[i].z * c;
            stars[i].y = ny;
            stars[i].z = nz;
            i = i + 1;
        }
        i = 0;
        while (i < 8)
        {
            if (enemies[i].alive != 0)
            {
                ny = enemies[i].y * c - enemies[i].z * s;
                nz = enemies[i].y * s + enemies[i].z * c;
                enemies[i].y = ny;
                enemies[i].z = nz;
            }
            i = i + 1;
        }
        if (ast_active != 0)
        {
            i = 0;
            while (i < ast_count)
            {
                ny = asteroids[i].y * c - asteroids[i].z * s;
                nz = asteroids[i].y * s + asteroids[i].z * c;
                asteroids[i].y = ny;
                asteroids[i].z = nz;
                i = i + 1;
            }
        }
        if (sb_active != 0)
        {
            ny = sb_y * c - sb_z * s;
            nz = sb_y * s + sb_z * c;
            sb_y = ny;
            sb_z = nz;
        }
        i = 0;
        while (i < DEBRIS_COUNT)
        {
            if (debris[i].t > 0)
            {
                ny = debris[i].y * c - debris[i].z * s;
                nz = debris[i].y * s + debris[i].z * c;
                debris[i].y = ny;
                debris[i].z = nz;
            }
            i = i + 1;
        }
    }

    // ------------------------------------------------------------------
    //  Zylon migration: about half of the hostile quadrants (excluding
    //  the one we are in) send one ship to a toroidally adjacent
    //  quadrant. Destinations turn hostile again (cleared reset).
    //  The starting quadrant is a valid destination, so its initial
    //  safety lasts only until the first migration.
    // ------------------------------------------------------------------

    void zylon_migration()
    {
        int i;
        int j;
        int dir;
        int tx;
        int ty;
        int cur;
        cur = qy * GALAXY_QUADS + qx;
        i = 0;
        while (i < GALAXY_QUADS * GALAXY_QUADS)
        {
            if (galaxy_map[i] > 0 && i != cur)
            {
                if (rng.between( 0, 1 ) == 0)
                {
                    tx = i % GALAXY_QUADS;
                    ty = i / GALAXY_QUADS;
                    dir = rng.between( 0, 3 );
                    if (dir == 0)
                        ty = ty - 1;
                    else if (dir == 1)
                        tx = tx + 1;
                    else if (dir == 2)
                        ty = ty + 1;
                    else
                        tx = tx - 1;
                    while (tx < 0)
                        tx = tx + GALAXY_QUADS;
                    while (tx >= GALAXY_QUADS)
                        tx = tx - GALAXY_QUADS;
                    while (ty < 0)
                        ty = ty + GALAXY_QUADS;
                    while (ty >= GALAXY_QUADS)
                        ty = ty - GALAXY_QUADS;
                    j = ty * GALAXY_QUADS + tx;
                    // never into the starbase quadrants or our own
                    if (j != cur && is_starbase_quad( tx, ty ) == 0)
                    {
                        galaxy_map[i] = galaxy_map[i] - 1;
                        if (galaxy_map[j] < 8)
                            galaxy_map[j] = galaxy_map[j] + 1;
                        cleared[j] = 0;
                    }
                }
            }
            i = i + 1;
        }
        move_t = MSG_FRAMES;
    }

    // ------------------------------------------------------------------
    //  chart navigation
    // ------------------------------------------------------------------

    int chart_dist( int fx, int fy, int tx, int ty )
    {
        int dx;
        int dy;
        dx = tx - fx;
        if (dx < 0)
            dx = -dx;
        if (dx > 4)
            dx = 8 - dx;
        dy = ty - fy;
        if (dy < 0)
            dy = -dy;
        if (dy > 4)
            dy = 8 - dy;
        return dx + dy;
    }

    void update_chart()
    {
        int d;
        int dist;
        float cost;

        d = gamepad_left();
        if (d > 0 && prev_bl <= 0)
            chart_cx = chart_cx - 1;
        prev_bl = d;
        d = gamepad_right();
        if (d > 0 && prev_br <= 0)
            chart_cx = chart_cx + 1;
        prev_br = d;
        d = gamepad_up();
        if (d > 0 && prev_bu <= 0)
            chart_cy = chart_cy - 1;
        prev_bu = d;
        d = gamepad_down();
        if (d > 0 && prev_bd <= 0)
            chart_cy = chart_cy + 1;
        prev_bd = d;

        if (chart_cx < 0)
            chart_cx = chart_cx + GALAXY_QUADS;
        if (chart_cx >= GALAXY_QUADS)
            chart_cx = chart_cx - GALAXY_QUADS;
        if (chart_cy < 0)
            chart_cy = chart_cy + GALAXY_QUADS;
        if (chart_cy >= GALAXY_QUADS)
            chart_cy = chart_cy - GALAXY_QUADS;

        d = gamepad_button_a();
        if (d > 0 && prev_ba <= 0)
        {
            dist = chart_dist( qx, qy, chart_cx, chart_cy );
            cost = WARP_COST + dist * 4;
            if (dist > 0 && energy >= cost)
            {
                energy = energy - cost;
                warp_to( chart_cx, chart_cy );
            }
        }
        prev_ba = d;
    }

    void warp_to( int nqx, int nqy )
    {
        // NOTE: we do NOT enter the destination yet -- the sector
        // (and anything hostile in it) only comes into existence
        // when we emerge from hyperspace at the end of the run
        target_qx = nqx;
        target_qy = nqy;
        warp_t = 1;
        warp_targeted = 1;
        chart_on = 0;
        nav_start_warp();
    }

    // ------------------------------------------------------------------

    void dock()
    {
        int i;
        shields = 100;
        energy = 100;
        i = 0;
        while (i < COMP_COUNT)
        {
            comp_hp[i] = 9;
            comp_pts[i] = 0;
            i = i + 1;
        }
        docked_t = MSG_FRAMES;
        dock_hint = 0;
        play_sound( SFX_REPLENISH );
    }

    // signed distance to the nearest object of a kind:
    // kind 0 = live Zylon, kind 1 = starbase (9999 = none)
    int nearest_dist( int kind )
    {
        int i;
        int best;
        float bestz;
        best = 9999;
        bestz = 0;
        if (kind == 0)
        {
            i = 0;
            while (i < 8)
            {
                if (enemies[i].alive != 0)
                {
                    float az;
                    az = enemies[i].z;
                    if (az < 0)
                        az = -az;
                    if (best == 9999 || az < bestz)
                    {
                        bestz = az;
                        best = enemies[i].z;
                    }
                }
                i = i + 1;
            }
        }
        else if (sb_active != 0)
        {
            best = sb_z;
        }
        return best;
    }

    // ------------------------------------------------------------------
    //  drawing
    // ------------------------------------------------------------------

    // screen X for a world-space lateral offset at depth d. The aft
    // view is a 180-degree yaw, which mirrors the horizontal axis.
    float view_sx( float x, float d )
    {
        if (aft_on != 0)
            return CENTER_X - x / d * FOCAL;
        return CENTER_X + x / d * FOCAL;
    }

    void draw()
    {
        if (screen == 0)
        {
            draw_title();
            return;
        }

        if (chart_on != 0)
        {
            draw_chart();
            return;
        }

        int i;
        float speed;
        float speedscale;

        speed = engine_speed();
        if (warp_t > 0)
            speed = WARP_SPEED;
        speedscale = speed / GEAR_SPEED;
        if (speedscale < 1)
            speedscale = 1;

        // GPU state reset: ClearScreen honors the active blending
        // mode and multiply color, so any state left over from the
        // previous frame would turn the clear into an additive
        // smear (trails building to a white-out). Always clear
        // with plain alpha + white multiply.
        set_blending_mode( blending_alpha );
        set_multiply_color( color_white );
        clear_screen( make_color_rgb( 2, 4, 12 ) );

        // stars
        select_texture( -1 );
        select_region( ASCII_DOT );
        set_blending_mode( blending_add );

        i = 0;
        while (i < STAR_COUNT)
        {
            float d;
            float sx;
            float sy;
            d = stars[i].z;
            if (aft_on != 0)
                d = 0 - d;
            if (d >= NEAR_Z)
            {
                sx = view_sx( stars[i].x, d );
                sy = CENTER_Y - stars[i].y / d * FOCAL;
                if (sx > -20 && sx < 660 && sy > -20 && sy < 380)
                {
                    int b;
                    int r;
                    int gg;
                    int bb;
                    float s;
                    b = 255 - d * 0.19;
                    if (b < 60)
                        b = 60;
                    r = b;
                    gg = b;
                    bb = b;
                    if (stars[i].tint == 1)
                    {
                        r = b * 0.7;
                        gg = b * 0.85;
                        bb = b + 40;
                    }
                    else if (stars[i].tint == 2)
                    {
                        r = b + 30;
                        gg = b * 0.8;
                        bb = b * 0.7;
                    }
                    else if (stars[i].tint == 3)
                    {
                        r = b * 0.7;
                        gg = b + 20;
                        bb = b + 30;
                    }
                    if (r > 255)
                        r = 255;
                    if (gg > 255)
                        gg = 255;
                    if (bb > 255)
                        bb = 255;

                    s = 420 / d * speedscale;
                    if (s < 0.5)
                        s = 0.5;
                    // keep stars star-sized: near ones must not swell
                    // into blobs that read as approaching objects
                    if (s > 3.5)
                        s = 3.5;

                    set_multiply_color( make_color_rgb( r, gg, bb ) );
                    draw_zoomed_centered( sx, sy, s );
                }
            }
            i = i + 1;
        }

        set_blending_mode( blending_alpha );

        // asteroids: chunky solid blocks, far to near
        if (ast_active != 0)
        {
            i = ast_count - 1;
            while (i >= 0)
            {
                float d;
                float sx;
                float sy;
                float s;
                d = asteroids[i].z;
                if (aft_on != 0)
                    d = 0 - d;
                if (d >= NEAR_Z)
                {
                    sx = view_sx( asteroids[i].x, d );
                    sy = CENTER_Y - asteroids[i].y / d * FOCAL;
                    s = 750 / d;
                    if (s < 0.5)
                        s = 0.5;
                    if (s > 6.0)
                        s = 6.0;
                    if (sx > -60 && sx < 700 && sy > -60 && sy < 420)
                    {
                        set_multiply_color( make_color_rgb( 150, 130, 110 ) );
                        draw_asteroid_sprite( sx, sy, s, asteroids[i].kind );
                    }
                }
                i = i - 1;
            }
        }

        // starbase: block-built station, stationary in space
        if (sb_active != 0 && (sb_z >= NEAR_Z || sb_z <= 0 - NEAR_Z))
        {
            float d;
            float sx;
            float sy;
            float s;
            d = sb_z;
            if (aft_on != 0)
                d = 0 - d;
            if (d >= NEAR_Z)
            {
                sx = view_sx( sb_x, d );
                sy = CENTER_Y - sb_y / d * FOCAL;
                s = 900 / d;
                if (s < 0.5)
                    s = 0.5;
                if (s > 4.0)
                    s = 4.0;
                if (sx > -80 && sx < 720 && sy > -80 && sy < 440)
                {
                    set_multiply_color( make_color_rgb( 80, 230, 230 ) );
                    draw_starbase_sprite( sx, sy, s );
                }
            }
        }

        // repair shuttle: lerps from the starbase's screen position
        // to our hull outbound, and back home on the return leg
        if (rep_state != 0 && sb_active != 0)
        {
            float d;
            float bsx;
            float bsy;
            float p;
            float px;
            float py;
            float s;
            d = sb_z;
            if (aft_on != 0)
                d = 0 - d;
            if (d >= NEAR_Z)
            {
                bsx = view_sx( sb_x, d );
                bsy = CENTER_Y - sb_y / d * FOCAL;
            }
            else
            {
                // base outside this view: shuttle comes from the
                // nearest screen edge
                bsx = CENTER_X;
                bsy = 0 - 40;
            }
            p = rep_t;
            p = p / REPAIR_STEPS;
            px = bsx + ( CENTER_X - bsx ) * p;
            py = bsy + ( CENTER_Y + 50 - bsy ) * p;
            s = 0.8 + p * 1.8;
            set_multiply_color( make_color_rgb( 255, 230, 120 ) );
            draw_repship_sprite( px, py, s );
        }

        // enemies (block-built Zylon cruisers), far to near
        i = 7;
        while (i >= 0)
        {
            if (enemies[i].alive != 0 && (enemies[i].z >= NEAR_Z || enemies[i].z <= 0 - NEAR_Z))
            {
                float d;
                float sx;
                float sy;
                float s;
                d = enemies[i].z;
                if (aft_on != 0)
                    d = 0 - d;
                sx = view_sx( enemies[i].x, d );
                sy = CENTER_Y - enemies[i].y / d * FOCAL;
                s = 700 / d;
                if (s < 0.5)
                    s = 0.5;
                if (s > 4.0)
                    s = 4.0;
                if (sx > -60 && sx < 700 && sy > -60 && sy < 420)
                {
                    if (enemies[i].kind == 1)
                        set_multiply_color( make_color_rgb( 255, 90, 70 ) );
                    else
                        set_multiply_color( make_color_rgb( 255, 180, 60 ) );
                    draw_zylon_sprite( sx, sy, s );
                }
            }
            i = i - 1;
        }

        // enemy bolts: red '*' growing as they close in
        i = 0;
        while (i < EBOLT_COUNT)
        {
            if (ebolts[i].active != 0 && (ebolts[i].z >= NEAR_Z || ebolts[i].z <= 0 - NEAR_Z))
            {
                float d;
                float sx;
                float sy;
                float s;
                d = ebolts[i].z;
                if (aft_on != 0)
                    d = 0 - d;
                sx = view_sx( ebolts[i].x, d );
                sy = CENTER_Y - ebolts[i].y / d * FOCAL;
                s = 500 / d;
                if (s < 0.8)
                    s = 0.8;
                if (s > 6.0)
                    s = 6.0;
                if (sx > -30 && sx < 670 && sy > -30 && sy < 390)
                {
                    select_region( ASCII_STAR );
                    set_multiply_color( make_color_rgb( 255, 70, 70 ) );
                    draw_zoomed_centered( sx, sy, s );
                }
            }
            i = i + 1;
        }

        // missiles: bright '*' heads with a fading tail dot, flying
        // down-range from the wing turrets toward the crosshair
        // (forward fire only: nothing to see in the aft view)
        if (aft_on == 0)
        {
            i = 0;
            while (i < MISSILE_COUNT)
            {
            if (missiles[i].active != 0)
            {
                float d;
                float sx;
                float sy;
                float s;
                float tx;
                float ty;
                d = missiles[i].z;
                sx = CENTER_X + missiles[i].x / d * FOCAL;
                sy = CENTER_Y - missiles[i].y / d * FOCAL;
                s = 550 / d;
                if (s < 1.2)
                    s = 1.2;
                if (s > 5.0)
                    s = 5.0;
                select_region( ASCII_STAR );
                // electric flicker: cycle 3 colors every few frames
                if (get_frame_counter() % 6 < 2)
                    set_multiply_color( make_color_rgb( 140, 255, 190 ) );
                else if (get_frame_counter() % 6 < 4)
                    set_multiply_color( make_color_rgb( 190, 200, 255 ) );
                else
                    set_multiply_color( make_color_rgb( 255, 255, 150 ) );
                draw_zoomed_centered( sx, sy, s );
                // tail: a dimmer dot one step back along the flight path
                d = d - MISSILE_STEP * 1.6;
                if (d > NEAR_Z)
                {
                    tx = CENTER_X + missiles[i].x / 0.93 / d * FOCAL;
                    ty = CENTER_Y - missiles[i].y / 0.93 / d * FOCAL;
                    set_multiply_color( make_color_rgba( 120, 255, 180, 110 ) );
                    draw_zoomed_centered( tx, ty, s * 0.7 );
                }
            }
            i = i + 1;
            }
        }

        // explosion debris: block fragments breaking apart from the
        // kill position, drifting with the debris velocity, fading out
        set_blending_mode( blending_alpha );
        i = 0;
        while (i < DEBRIS_COUNT)
        {
            if (debris[i].t > 0)
            {
                float d;
                float sx;
                float sy;
                float s;
                int a;
                d = debris[i].z;
                if (aft_on != 0)
                    d = 0 - d;
                if (d >= NEAR_Z)
                {
                    sx = view_sx( debris[i].x, d );
                    sy = CENTER_Y - debris[i].y / d * FOCAL;
                    s = 420 / d;
                    if (s < 0.5)
                        s = 0.5;
                    if (s > 5.0)
                        s = 5.0;
                    a = debris[i].t * 255 / debris[i].life;
                    if (a > 255)
                        a = 255;
                    if (sx > -60 && sx < 700 && sy > -60 && sy < 420)
                    {
                        select_region( ASCII_BLK1 + debris[i].kind );
                        set_multiply_color( make_color_rgba( 255, 170, 60, a ) );
                        // blocks are tall (10x20): flatten like rocks
                        draw_zoomed_rect( sx, sy, s, s * 0.6 );
                    }
                }
            }
            i = i + 1;
        }

        draw_hud();
        draw_radar();

        // damage flash: translucent red full-screen overlay
        // (full-solid block glyph: it fills its whole cell, unlike
        // the dash which only paints a thin band)
        if (flash_t > 0)
        {
            select_region( ASCII_BLK4 );
            set_multiply_color( make_color_rgba( 255, 40, 40, flash_t * 30 ) );
            set_drawing_scale( 64.0, 18.0 );
            draw_region_zoomed_at( 0, 0 );
            set_multiply_color( color_white );
        }

        // shield tint: light blue while up; the more damaged the
        // shield SYSTEM, the more it flickers
        if (shields_on != 0)
        {
            int show;
            int ontime;
            show = 1;
            ontime = 16 - ( 9 - comp_hp[COMP_SHD] ) * 2;
            if (ontime < 2)
                ontime = 2;
            if (comp_hp[COMP_SHD] < 9)
                show = (get_frame_counter() % 16) < ontime;
            if (show != 0)
            {
                select_region( ASCII_BLK4 );
                set_multiply_color( make_color_rgba( 70, 130, 255, 34 ) );
                set_drawing_scale( 64.0, 18.0 );
                draw_region_zoomed_at( 0, 0 );
            }
            set_multiply_color( color_white );
        }

        // pause overlay
        if (pause_on != 0)
        {
            select_region( ASCII_BLK4 );
            set_multiply_color( make_color_rgba( 0, 0, 0, 170 ) );
            set_drawing_scale( 64.0, 18.0 );
            draw_region_zoomed_at( 0, 0 );
            set_multiply_color( color_white );
            print_at( CENTER_X - 30, CENTER_Y - 50, s_pause );
            print_at( CENTER_X - 80, CENTER_Y - 10, s_ph1 );
            print_at( CENTER_X - 80, CENTER_Y + 20, s_ph2 );
        }

        // hyperspace course marker: an amber diamond the pilot must
        // align with the reticle during the warp acceleration
        if (warp_t > 0 && computer_on != 0)
        {
            float mx;
            float my;
            float ms;
            mx = CENTER_X + mk_yaw * FOCAL;
            my = CENTER_Y - mk_pitch * FOCAL;
            ms = 0.9 + 0.15 * ( get_frame_counter() % 20 );
            select_region( ASCII_PLUS );
            set_multiply_color( make_color_rgb( 255, 200, 40 ) );
            draw_zoomed_centered( mx, my, ms );
            draw_zoomed_centered( mx, my, ms * 0.55 );
        }

        // reticle last: nothing may draw over the aiming cross
        draw_reticle();

        // and leave the GPU in a clean state for the next frame's
        // clear_screen (see the note at the top of draw())
        set_blending_mode( blending_alpha );
        set_multiply_color( color_white );
    }

    // ------------------------------------------------------------------
    //  composite sprites, built from the block glyphs 0x11-0x14
    // ------------------------------------------------------------------

    // Asteroid: a bulbous lump -- a big core block with smaller
    // blocks welded on at jittered offsets, so it reads as a
    // chunky rock instead of a square. 'kind' picks a silhouette.
    // Multiply color is set by the caller.
    void draw_asteroid_sprite( int sx, int sy, float s, int kind )
    {
        if (kind == 0 || kind == 2)
        {
            select_region( ASCII_BLK3 );
            draw_zoomed_rect( sx, sy, 1.4 * s, 0.8 * s );
            select_region( ASCII_BLK2 );
            draw_zoomed_rect( sx - 10 * s, sy + 3 * s, 0.8 * s, 0.5 * s );
            draw_zoomed_rect( sx + 9 * s, sy - 4 * s, 0.7 * s, 0.45 * s );
            select_region( ASCII_BLK1 );
            draw_zoomed_rect( sx + 4 * s, sy - 6 * s, 0.5 * s, 0.3 * s );
        }
        else
        {
            select_region( ASCII_BLK3 );
            draw_zoomed_rect( sx, sy, 1.2 * s, 0.9 * s );
            select_region( ASCII_BLK2 );
            draw_zoomed_rect( sx + 11 * s, sy + 4 * s, 0.75 * s, 0.5 * s );
            draw_zoomed_rect( sx - 9 * s, sy - 5 * s, 0.65 * s, 0.4 * s );
            draw_zoomed_rect( sx - 2 * s, sy + 8 * s, 0.6 * s, 0.35 * s );
            select_region( ASCII_BLK1 );
            draw_zoomed_rect( sx - 5 * s, sy - 7 * s, 0.45 * s, 0.28 * s );
        }
    }

    // repair shuttle: a small yellow workbee that flies from the
    // starbase to our ship. Body, side pods, beacon light.
    void draw_repship_sprite( int sx, int sy, float s )
    {
        select_region( ASCII_BLK4 );
        draw_zoomed_rect( sx, sy, 0.8 * s, 0.5 * s );
        select_region( ASCII_BLK2 );
        draw_zoomed_rect( sx - 7 * s, sy + 2 * s, 0.6 * s, 0.35 * s );
        draw_zoomed_rect( sx + 7 * s, sy + 2 * s, 0.6 * s, 0.35 * s );
        select_region( ASCII_BLK1 );
        draw_zoomed_rect( sx, sy - 5 * s, 0.4 * s, 0.25 * s );
    }

    // Zylon cruiser: full-solid body, mid-solid swept wings,
    // light-solid cockpit. Multiply color is set by the caller.
    void draw_zylon_sprite( int sx, int sy, float s )
    {
        select_region( ASCII_BLK3 );
        draw_zoomed_rect( sx - 13 * s, sy + 3 * s, 0.9 * s, 0.5 * s );
        draw_zoomed_rect( sx + 13 * s, sy + 3 * s, 0.9 * s, 0.5 * s );
        select_region( ASCII_BLK4 );
        draw_zoomed_rect( sx, sy, 1.5 * s, 0.8 * s );
        select_region( ASCII_BLK1 );
        draw_zoomed_rect( sx, sy - 7 * s, 0.6 * s, 0.35 * s );
    }

    // Starbase: full-solid core, mid-solid docking arms, light-solid
    // beacon lights on the corners
    void draw_starbase_sprite( int sx, int sy, float s )
    {
        select_region( ASCII_BLK2 );
        draw_zoomed_rect( sx, sy - 15 * s, 0.9 * s, 0.8 * s );
        draw_zoomed_rect( sx, sy + 15 * s, 0.9 * s, 0.8 * s );
        draw_zoomed_rect( sx - 15 * s, sy, 0.8 * s, 0.9 * s );
        draw_zoomed_rect( sx + 15 * s, sy, 0.8 * s, 0.9 * s );
        select_region( ASCII_BLK4 );
        draw_zoomed_rect( sx, sy, 1.5 * s, 1.1 * s );
        select_region( ASCII_BLK1 );
        draw_zoomed_rect( sx - 15 * s, sy - 15 * s, 0.5 * s, 0.4 * s );
        draw_zoomed_rect( sx + 15 * s, sy - 15 * s, 0.5 * s, 0.4 * s );
        draw_zoomed_rect( sx - 15 * s, sy + 15 * s, 0.5 * s, 0.4 * s );
        draw_zoomed_rect( sx + 15 * s, sy + 15 * s, 0.5 * s, 0.4 * s );
    }

    void draw_reticle()
    {
        // no reticle at all without the attack computer
        if (computer_on == 0)
            return;

        // fully self-contained draw state; additive so it glows.
        // NOTE: stretching a dash glyph vertically does NOT make a
        // vertical line -- the dash occupies only ~3 rows of its
        // 20-pixel cell, so a Y-scaled dash is just a small blob.
        // Vertical bars must use the '|' pipe glyph.
        select_texture( -1 );
        set_blending_mode( blending_add );
        set_multiply_color( make_color_rgb( 0, 220, 130 ) );
        if (aft_on != 0)
        {
            // aft view: two thick horizontal hairs with a wide gap
            // between them (no vertical -- we are not aiming)
            select_region( ASCII_DASH );
            set_drawing_scale( 2.2, 1.0 );
            draw_region_zoomed_at( CENTER_X - 62, CENTER_Y - 10 );
            draw_region_zoomed_at( CENTER_X + 40, CENTER_Y - 10 );
        }
        else
        {
            // full cross: dash for the horizontal hair, pipe for
            // the vertical bar
            select_region( ASCII_DASH );
            set_drawing_scale( 3.0, 1.0 );
            draw_region_zoomed_at( CENTER_X - 15, CENTER_Y - 10 );
            select_region( ASCII_PIPE );
            set_drawing_scale( 1.0, 1.4 );
            draw_region_zoomed_at( CENTER_X - 5, CENTER_Y - 14 );
        }
    }

    // 9-position damage gauge: 3 red / 3 yellow / 3 green cells,
    // filled up to the current health
    void draw_gauge( int x, int y, int hp )
    {
        int i;
        i = 0;
        while (i < 9)
        {
            select_region( ASCII_BLK4 );
            if (i < hp)
            {
                if (i < 3)
                    set_multiply_color( make_color_rgb( 230, 60, 50 ) );
                else if (i < 6)
                    set_multiply_color( make_color_rgb( 235, 200, 50 ) );
                else
                    set_multiply_color( make_color_rgb( 70, 220, 90 ) );
            }
            else
                set_multiply_color( make_color_rgb( 35, 35, 45 ) );
            set_drawing_scale( 1.0, 0.45 );
            draw_region_zoomed_at( x + i * 12, y );
            i = i + 1;
        }
        set_multiply_color( color_white );
    }

    // horizontal bar: the '-' glyph zoomed non-uniformly
    void draw_bar( int x, int y, float frac, int r, int gg, int b )
    {
        select_region( ASCII_DASH );
        set_multiply_color( make_color_rgb( 30, 30, 40 ) );
        set_drawing_scale( 20.0, 0.5 );
        draw_region_zoomed_at( x, y );
        if (frac < 0)
            frac = 0;
        if (frac > 1)
            frac = 1;
        set_multiply_color( make_color_rgb( r, gg, b ) );
        set_drawing_scale( 20.0 * frac, 0.5 );
        draw_region_zoomed_at( x, y );
    }

    // short-range radar (bottom-right): direction of each Zylon (red),
    // the starbase (cyan), asteroids (gray)
    void draw_radar()
    {
        int rcx;
        int rcy;
        int range;
        int i;
        rcx = 560;
        rcy = 296;
        range = 52;

        select_region( ASCII_DASH );
        set_multiply_color( make_color_rgb( 60, 90, 80 ) );
        set_drawing_scale( 11.0, 0.3 );
        draw_region_zoomed_at( rcx - 55, rcy - 57 );
        draw_region_zoomed_at( rcx - 55, rcy + 55 );
        select_region( ASCII_PIPE );
        set_drawing_scale( 0.3, 5.4 );
        draw_region_zoomed_at( rcx - 57, rcy - 54 );
        draw_region_zoomed_at( rcx + 55, rcy - 54 );

        // us
        select_region( ASCII_PLUS );
        set_multiply_color( make_color_rgb( 0, 180, 110 ) );
        draw_zoomed_centered( rcx, rcy, 0.6 );

        // asteroids
        if (ast_active != 0)
        {
            i = 0;
            while (i < ast_count)
            {
                float nx;
                float ny;
                float ez;
                ez = asteroids[i].z;
                if (aft_on != 0)
                    ez = 0 - ez;
                if (ez >= NEAR_Z)
                {
                    nx = asteroids[i].x / ez / 0.9;
                    ny = 0 - asteroids[i].y / ez / 0.9;
                    if (aft_on != 0)
                        nx = 0 - nx;
                    if (nx > 1)
                        nx = 1;
                    if (nx < -1)
                        nx = -1;
                    if (ny > 1)
                        ny = 1;
                    if (ny < -1)
                        ny = -1;
                    select_region( ASCII_DOT );
                    set_multiply_color( make_color_rgb( 150, 140, 120 ) );
                    draw_zoomed_centered( rcx + nx * range, rcy + ny * range, 1.0 );
                }
                i = i + 1;
            }
        }

        // enemies
        i = 0;
        while (i < 8)
        {
            if (enemies[i].alive != 0)
            {
                float nx;
                float ny;
                float ez;
                ez = enemies[i].z;
                if (aft_on != 0)
                    ez = 0 - ez;
                if (ez >= NEAR_Z)
                {
                    nx = enemies[i].x / ez / 0.9;
                    ny = 0 - enemies[i].y / ez / 0.9;
                    if (aft_on != 0)
                        nx = 0 - nx;
                    if (nx > 1)
                        nx = 1;
                    if (nx < -1)
                        nx = -1;
                    if (ny > 1)
                        ny = 1;
                    if (ny < -1)
                        ny = -1;
                    select_region( ASCII_DOT );
                    set_multiply_color( make_color_rgb( 255, 80, 80 ) );
                    draw_zoomed_centered( rcx + nx * range, rcy + ny * range, 1.2 );
                }
            }
            i = i + 1;
        }

        // starbase
        if (sb_active != 0)
        {
            float nx;
            float ny;
            float ez;
            ez = sb_z;
            if (aft_on != 0)
                ez = 0 - ez;
            if (ez >= NEAR_Z)
            {
                nx = sb_x / ez / 0.9;
                ny = 0 - sb_y / ez / 0.9;
                if (aft_on != 0)
                    nx = 0 - nx;
                if (nx > 1)
                    nx = 1;
                if (nx < -1)
                    nx = -1;
                if (ny > 1)
                    ny = 1;
                if (ny < -1)
                    ny = -1;
                select_region( ASCII_DOT );
                set_multiply_color( make_color_rgb( 80, 230, 230 ) );
                draw_zoomed_centered( rcx + nx * range, rcy + ny * range, 1.6 );
            }
        }
    }

    // galactic chart: 8x8 grid + ship status panel + cursor
    void draw_chart()
    {
        int gx;
        int gy;
        int cell_x;
        int cell_y;
        int cell;
        int hostile;

        // clean GPU state before clearing (see note in draw())
        set_blending_mode( blending_alpha );
        set_multiply_color( color_white );
        clear_screen( make_color_rgb( 2, 4, 12 ) );

        set_multiply_color( color_white );
        print_at( 260, 30, s_chart );

        strcpy( hud_line, s_cost );
        hud_append_int( WARP_COST + chart_dist( qx, qy, chart_cx, chart_cy ) * 4 );
        print_at( 440, 30, hud_line );

        strcpy( hud_line, s_yclose );
        strcat( hud_line, s_awarp );
        print_at( 260, 320, hud_line );

        cell_x = 240;
        cell_y = 80;
        for (gy = 0; gy < GALAXY_QUADS; gy++)
        {
            for (gx = 0; gx < GALAXY_QUADS; gx++)
            {
                cell = gy * GALAXY_QUADS + gx;
                hostile = galaxy_map[cell];
                if (cleared[cell] != 0)
                    hostile = 0;
                if (gx == qx && gy == qy)
                {
                    select_region( ASCII_PLUS );
                    set_multiply_color( make_color_rgb( 0, 220, 120 ) );
                    draw_zoomed_centered( cell_x, cell_y, 1.0 );
                }
                else if (is_starbase_quad( gx, gy ) != 0)
                {
                    select_region( ASCII_S );
                    set_multiply_color( make_color_rgb( 80, 230, 230 ) );
                    draw_zoomed_centered( cell_x, cell_y, 1.0 );
                }
                else if (hostile > 0)
                {
                    select_region( ASCII_V );
                    set_multiply_color( make_color_rgb( 255, 90, 70 ) );
                    draw_zoomed_centered( cell_x, cell_y, 1.0 );
                    // hostile AND an asteroid field: rock marker beside
                    if (ast_map[cell] > 0)
                    {
                        select_region( ASCII_BLK3 );
                        set_multiply_color( make_color_rgb( 120, 105, 90 ) );
                        draw_zoomed_rect( cell_x + 9, cell_y, 0.5, 0.3 );
                    }
                }
                else if (ast_map[cell] > 0)
                {
                    // asteroid field: a dim solid block
                    select_region( ASCII_BLK3 );
                    set_multiply_color( make_color_rgb( 120, 105, 90 ) );
                    draw_zoomed_rect( cell_x, cell_y, 0.8, 0.45 );
                }
                else
                {
                    select_region( ASCII_DOT );
                    set_multiply_color( make_color_rgb( 90, 90, 110 ) );
                    draw_zoomed_centered( cell_x, cell_y, 1.0 );
                }
                cell_x = cell_x + 20;
            }
            cell_x = 240;
            cell_y = cell_y + 25;
        }

        // selection cursor: white brackets around the chosen cell
        {
            int bx;
            int by;
            bx = 240 + chart_cx * 20;
            by = 80 + chart_cy * 25;
            select_region( ASCII_DASH );
            set_multiply_color( make_color_rgb( 255, 255, 255 ) );
            set_drawing_scale( 1.8, 0.15 );
            draw_region_zoomed_at( bx - 8, by - 10 );
            draw_region_zoomed_at( bx - 8, by + 8 );
            select_region( ASCII_PIPE );
            set_drawing_scale( 0.15, 0.95 );
            draw_region_zoomed_at( bx - 9, by - 9 );
            draw_region_zoomed_at( bx + 9, by - 9 );
        }

        // ship system status panel (left side)
        set_multiply_color( color_white );
        print_at( 8, 80, s_energy );
        draw_bar( 8, 102, energy / 100, 80, 220, 80 );
        print_at( 8, 126, s_shields );
        draw_bar( 8, 148, shields / 100, 80, 140, 255 );
        print_at( 8, 172, s_spd );
        draw_bar( 8, 194, gear_frac(), 255, 200, 40 );
        // component status gauges
        print_at( 8, 218, s_leng );
        draw_gauge( 60, 226, comp_hp[COMP_ENG] );
        print_at( 8, 240, s_lshd );
        draw_gauge( 60, 248, comp_hp[COMP_SHD] );
        print_at( 8, 262, s_lcmp );
        draw_gauge( 60, 270, comp_hp[COMP_CMP] );
        print_at( 8, 284, s_lcan );
        draw_gauge( 60, 292, comp_hp[COMP_CAN] );
        if (shields_on != 0)
            print_at( 200, 218, s_sh_on );
        else
            print_at( 200, 218, s_sh_off );
        if (computer_on != 0)
            print_at( 200, 240, s_ac_on );
        else
            print_at( 200, 240, s_ac_off );
        strcpy( hud_line, s_near );
        strcat( hud_line, s_zch );
        strcat( hud_line, s_sp );
        hud_append_int( nearest_dist( 0 ) );
        strcat( hud_line, s_sp );
        strcat( hud_line, s_bch );
        strcat( hud_line, s_sp );
        hud_append_int( nearest_dist( 1 ) );
        print_at( 8, 306, hud_line );

        // asteroid field notice for the selected quadrant
        if (ast_map[chart_cy * GALAXY_QUADS + chart_cx] > 0)
            print_at( 240, 58, s_astfield );
    }

    // throttle bar fraction: gear -1..8 maps to 0.1..1.0
    float gear_frac()
    {
        return (gear - GEAR_MIN + 1) * 0.1;
    }

    void draw_hud()
    {
        set_multiply_color( color_white );

        // top-left: position + enemy count + navigation
        strcpy( hud_line, s_quadrant );
        hud_append_int( qx + 1 );
        strcat( hud_line, s_comma );
        hud_append_int( qy + 1 );
        strcat( hud_line, s_sector );
        hud_append_int( sector_x() + 1 );
        strcat( hud_line, s_comma );
        hud_append_int( sector_y() + 1 );
        print_at( 8, 8, hud_line );

        strcpy( hud_line, s_zylons );
        hud_append_int( enemies_alive() );
        print_at( 8, 32, hud_line );

        strcpy( hud_line, s_hdg );
        hud_append_int( nav_heading_deg() );
        strcat( hud_line, s_pit );
        hud_append_int( nav_pitch_deg() );
        strcat( hud_line, s_pos );
        hud_append_int( shipx );
        strcat( hud_line, s_comma );
        hud_append_int( shipy );
        print_at( 8, 56, hud_line );

        // nearest object: type + signed distance
        strcpy( hud_line, s_near );
        strcat( hud_line, s_zch );
        strcat( hud_line, s_sp );
        hud_append_int( nearest_dist( 0 ) );
        strcat( hud_line, s_sp );
        strcat( hud_line, s_bch );
        strcat( hud_line, s_sp );
        hud_append_int( nearest_dist( 1 ) );
        print_at( 8, 80, hud_line );

        // top-right: energy and shield bars
        print_at( 440, 8, s_energy );
        draw_bar( 440, 32, energy / 100, 80, 220, 80 );
        print_at( 440, 56, s_shields );
        draw_bar( 440, 80, shields / 100, 80, 140, 255 );

        // engine speed bar
        print_at( 440, 104, s_spd );
        draw_bar( 440, 128, gear_frac(), 255, 200, 40 );

        // shields / attack computer state
        if (shields_on != 0)
            print_at( 440, 152, s_sh_on );
        else
            print_at( 440, 152, s_sh_off );
        if (computer_on != 0)
            print_at( 440, 176, s_ac_on );
        else
            print_at( 440, 176, s_ac_off );
        if (aft_on != 0)
            print_at( 440, 200, s_aft );
        else
            print_at( 440, 200, s_fwd );

        // bottom-left: engines + heading
        if (warp_t > 0)
            strcpy( hud_line, s_warp );
        else if (gear < 0)
            strcpy( hud_line, s_rev );
        else if (gear == 0)
            strcpy( hud_line, s_stop );
        else
            strcpy( hud_line, s_cruise );
        strcat( hud_line, s_head );
        if (heading() == 0)
            strcat( hud_line, s_n );
        else if (heading() == 1)
            strcat( hud_line, s_e );
        else if (heading() == 2)
            strcat( hud_line, s_s );
        else
            strcat( hud_line, s_w );
        print_at( 8, 332, hud_line );

        // docking hint: near the base but not stopped
        if (dock_hint != 0 && docked_t == 0)
            print_at( CENTER_X - 110, CENTER_Y + 60, s_fullstop );

        // center messages
        if (dead != 0)
        {
            print_at( CENTER_X - 60, CENTER_Y - 40, s_destroyed );
            print_at( CENTER_X - 50, CENTER_Y - 10, s_restart );
        }
        else if (docked_t > 0)
        {
            print_at( CENTER_X - 30, CENTER_Y - 40, s_docked );
            if (rep_had_dmg != 0)
                print_at( CENTER_X - 75, CENTER_Y - 10, s_repcomp );
            else
                print_at( CENTER_X - 80, CENTER_Y - 10, s_energyrep );
        }
        else if (msg_t > 0)
        {
            print_at( CENTER_X - 55, CENTER_Y - 40, s_clear );
        if (move_t > 0)
            print_at( CENTER_X - 60, CENTER_Y - 70, s_move );
        }
        if (offc_t > 0)
            print_at( CENTER_X - 45, CENTER_Y - 70, s_offcourse );
    }

    int sector_x()
    {
        int s;
        s = shipx / (QUAD_SIZE / SECTORS_PER_QUAD);
        if (s < 0)
            s = 0;
        if (s > SECTORS_PER_QUAD - 1)
            s = SECTORS_PER_QUAD - 1;
        return s;
    }

    int sector_y()
    {
        int s;
        s = shipy / (QUAD_SIZE / SECTORS_PER_QUAD);
        if (s < 0)
            s = 0;
        if (s > SECTORS_PER_QUAD - 1)
            s = SECTORS_PER_QUAD - 1;
        return s;
    }
};

Starfield g_field;

// ---------------------------------------------------------------------------
//  Main loop
// ---------------------------------------------------------------------------

void main( void )
{
    g_field.init_title();

    while (1)
    {
        g_field.update();
        g_field.draw();
        end_frame();
    }
}
