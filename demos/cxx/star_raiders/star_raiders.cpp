#title "Star Raiders"
#version 0.6

#include "video.h"
#include "input.h"
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
#define STAR_COUNT        140
#define NEAR_Z             50
#define FAR_Z            1050
#define DEPTH_Z           990
#define FIELD_XY          700
#define FOCAL             300

// engines: gear -1 (slow reverse) .. 8 (fastest sublight)
#define GEAR_MIN           -1
#define GEAR_MAX            8
#define GEAR_SPEED          3    // units per frame per gear step

#define WARP_FRAMES        90
#define WARP_SPEED         60
#define TURN_RATE        0.035

// missiles (one fired per X press, alternating cannons)
#define MISSILE_COUNT       6
#define MISSILE_STEP       55    // z advance per frame
#define MISSILE_COST      1.0
#define MISSILE_LATERAL   45    // turret offset from the view axis

// starbases: fixed quadrants, repair needs full stop at close range
#define DOCK_RANGE        130
#define DOCK_ZMIN          40

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

// shields & attack computer
#define SHIELD_DRAIN     0.012

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
int s_repair[17]    = { 83, 89, 83, 84, 69, 77, 83, 32, 82, 69, 83, 84, 79, 82, 69, 68, 0 }; // "SYSTEMS RESTORED"
int s_cost[7]       = { 67, 79, 83, 84, 58, 32, 0 };                             // "COST: "
int s_awarp[10]     = { 32, 32, 65, 58, 32, 87, 65, 82, 80, 0 };                 // "  A: WARP"
int s_sh_on[9]      = { 83, 72, 76, 68, 58, 32, 79, 78, 0 };                     // "SHLD: ON"
int s_sh_off[10]    = { 83, 72, 76, 68, 58, 32, 79, 70, 70, 0 };                 // "SHLD: OFF"
int s_ac_on[9]      = { 65, 67, 77, 80, 58, 32, 79, 78, 0 };                     // "ACMP: ON"
int s_ac_off[10]    = { 65, 67, 77, 80, 58, 32, 79, 70, 70, 0 };                 // "ACMP: OFF"
int s_near[7]       = { 78, 69, 65, 82, 58, 32, 0 };                             // "NEAR: "
int s_sp[2]         = { 32, 0 };                                                 // " "
int s_zch[2]        = { 90, 0 };                                                 // "Z"
int s_bch[2]        = { 83, 0 };                                                 // "S"
int s_pause[7]      = { 80, 65, 85, 83, 69, 68, 0 };                             // "PAUSED"
int s_ph1[16]       = { 83, 84, 65, 82, 84, 43, 65, 58, 32, 65, 84, 84, 65, 67, 75, 0 }; // "START+A: ATTACK"
int s_ph2[17]       = { 83, 84, 65, 82, 84, 43, 66, 58, 32, 83, 72, 73, 69, 76, 68, 83, 0 }; // "START+B: SHIELDS"
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
        z = rng->between( NEAR_Z + 40, FAR_Z );
        tint = rng->between( 0, 3 );
    }

    // respawn at a uniformly random depth (a fixed distance would
    // bunch stars into a thin shell that whips past at warp speed)
    void respawn( RNG *rng )
    {
        x = rng->between( -FIELD_XY, FIELD_XY );
        y = rng->between( -FIELD_XY, FIELD_XY );
        z = NEAR_Z + 10 + rng->between( 0, DEPTH_Z - 20 );
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
//  Explosion: screen-space marker of a kill
// ---------------------------------------------------------------------------

class Explosion
{
public:
    int sx;
    int sy;
    int t;
};

// ---------------------------------------------------------------------------
//  The game
// ---------------------------------------------------------------------------

class Starfield
{
public:
    Star stars[140];
    Enemy enemies[8];
    Asteroid asteroids[8];
    Missile missiles[6];
    EnemyBolt ebolts[8];
    Explosion explosions[4];
    RNG rng;

    int qx;
    int qy;
    float shipx;
    float shipy;
    float yaw;
    float pitch;
    int warp_t;
    int warp_dir;
    int warp_targeted;

    float energy;
    float shields;
    int kills;
    int msg_t;
    int dead;

    int cannons;        // 2 = both, 1 = right only, 0 = none
    int engine_dmg;
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
    int shield_dmg;

    int sb_active;
    float sb_x;
    float sb_y;
    float sb_z;
    int docked_t;
    int dock_hint;      // 1 = near base but not stopped

    int ast_active;     // asteroid field in this quadrant
    int ast_count;

    int chart_on;
    int prev_y;
    int galaxy_map[64];
    int cleared[64];
    int ast_map[64];    // asteroid fields per quadrant

    int chart_cx;
    int chart_cy;
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
        rng.seed( (cqy * GALAXY_QUADS + cqx + 1) * 7919 );
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
        rng.seed( (qy * GALAXY_QUADS + qx + 1) * 7919 );
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
        i = 0;
        while (i < 4)
        {
            explosions[i].t = 0;
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
    }

    void init( int start_qx, int start_qy )
    {
        int i;
        yaw = 0;
        pitch = 0;
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
        cannons = 2;
        engine_dmg = 0;
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
        shields_on = 1;
        shield_dmg = 0;
        chart_on = 0;
        prev_y = 0;
        chart_cx = start_qx;
        chart_cy = start_qy;
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

    void update()
    {
        float turn;
        float speed;
        int i;
        int d;

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
                computer_on = 1 - computer_on;
                start_combo = 1;
            }
            if (gamepad_button_b() > 0 && prev_bb <= 0)
            {
                shields_on = 1 - shields_on;
                start_combo = 1;
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

        if (chart_on != 0)
        {
            update_chart();
            return;
        }

        // view rotation: left/right = yaw, up/down = pitch
        turn = 0;
        if (gamepad_left() > 0)
            turn = turn - TURN_RATE;
        if (gamepad_right() > 0)
            turn = turn + TURN_RATE;
        if (turn != 0)
        {
            yaw = yaw + turn;
            rotate_yaw( turn );
        }
        if (gamepad_up() > 0)
        {
            rotate_pitch( 0 - TURN_RATE );
            pitch = pitch - TURN_RATE;
        }
        if (gamepad_down() > 0)
        {
            rotate_pitch( TURN_RATE );
            pitch = pitch + TURN_RATE;
        }
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
        if (engine_dmg != 0)
            speed = speed * 0.5;

        // warp on A (not while Start is held: that is a combo)
        if (warp_t == 0)
        {
            if (gamepad_button_a() > 0 && gamepad_button_start() <= 0 &&
                energy >= WARP_COST)
            {
                energy = energy - WARP_COST;
                warp_t = 1;
                warp_dir = heading();
            }
        }
        else
        {
            speed = WARP_SPEED;
            warp_t = warp_t + 1;
            if (warp_t > WARP_FRAMES)
            {
                warp_t = 0;
                if (warp_targeted != 0)
                    warp_targeted = 0;
                else
                    arrive();
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

        // stars stream past (both directions: reverse pushes them away)
        i = 0;
        while (i < STAR_COUNT)
        {
            stars[i].z = stars[i].z - speed;
            if (stars[i].z < NEAR_Z)
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
                if (asteroids[i].z < NEAR_Z)
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

        // starbase: stationary; docking needs full stop at close range
        if (sb_active != 0)
        {
            sb_z = sb_z - speed;
            if (sb_z < DOCK_ZMIN && sb_z > 0 - DOCK_ZMIN &&
                sb_x > -DOCK_RANGE && sb_x < DOCK_RANGE &&
                sb_y > -DOCK_RANGE && sb_y < DOCK_RANGE)
            {
                if (gear == 0)
                {
                    dock();
                }
                else
                    dock_hint = 1;
            }
            else
                dock_hint = 0;
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

        // explosions decay
        i = 0;
        while (i < 4)
        {
            if (explosions[i].t > 0)
                explosions[i].t = explosions[i].t - 1;
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
        if (docked_t > 0)
            docked_t = docked_t - 1;
    }

    // ------------------------------------------------------------------
    //  missiles
    // ------------------------------------------------------------------

    void fire_missile()
    {
        int i;
        int side;
        if (cannons <= 0 || energy < MISSILE_COST)
            return;
        // pick the cannon: with both, alternate; with one, right only
        if (cannons == 2)
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
                missiles[i].y = -25;
                missiles[i].z = NEAR_Z + 30;
                missiles[i].side = side;
                missiles[i].active = 1;
                energy = energy - MISSILE_COST;
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
        int slot;
        float d;
        slot = 0;
        if (explosions[1].t < explosions[slot].t)
            slot = 1;
        if (explosions[2].t < explosions[slot].t)
            slot = 2;
        if (explosions[3].t < explosions[slot].t)
            slot = 3;
        d = ez;
        if (d < NEAR_Z)
            d = NEAR_Z;
        explosions[slot].sx = CENTER_X + ex / d * FOCAL;
        explosions[slot].sy = CENTER_Y - ey / d * FOCAL;
        explosions[slot].t = 20;
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
        int roll;
        flash_t = FLASH_FRAMES;
        if (shields_on == 0)
        {
            dead = 1;
            return;
        }
        shields = shields - dmg;
        if (shields <= 0)
        {
            shields = 0;
            dead = 1;
            return;
        }
        roll = rng.between( 1, CANNON_HIT_ODDS + ENGINE_HIT_ODDS + SHIELD_HIT_ODDS );
        if (roll <= CANNON_HIT_ODDS)
        {
            if (cannons > 0)
                cannons = cannons - 1;
        }
        else if (roll <= CANNON_HIT_ODDS + ENGINE_HIT_ODDS)
        {
            engine_dmg = 1;
        }
        else
        {
            shield_dmg = 1;
        }
    }

    void update_enemies()
    {
        int i;
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
                        if (enemies[i].kind == 1)
                            enemies[i].cd = rng.between( 50, 100 );
                        else
                            enemies[i].cd = rng.between( 80, 160 );
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
        shipx = QUAD_HALF;
        shipy = QUAD_HALF;
        enter_quadrant( nqx, nqy );
        warp_t = 1;
        warp_targeted = 1;
        chart_on = 0;
    }

    // ------------------------------------------------------------------

    void dock()
    {
        shields = 100;
        energy = 100;
        cannons = 2;
        engine_dmg = 0;
        shield_dmg = 0;
        docked_t = MSG_FRAMES;
        dock_hint = 0;
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

    void draw()
    {
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
            if (d >= NEAR_Z)
            {
                sx = CENTER_X + stars[i].x / d * FOCAL;
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
                    if (s > 10.0)
                        s = 10.0;

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
                if (d >= NEAR_Z)
                {
                    sx = CENTER_X + asteroids[i].x / d * FOCAL;
                    sy = CENTER_Y - asteroids[i].y / d * FOCAL;
                    s = 750 / d;
                    if (s < 0.5)
                        s = 0.5;
                    if (s > 6.0)
                        s = 6.0;
                    if (sx > -60 && sx < 700 && sy > -60 && sy < 420)
                    {
                        select_region( ASCII_BLK1 + asteroids[i].kind );
                        set_multiply_color( make_color_rgb( 150, 130, 110 ) );
                        // blocks are tall (10x20): flatten to look rocky
                        draw_zoomed_rect( sx, sy, s * asteroids[i].asp,
                                          s * 0.62 * asteroids[i].asp );
                    }
                }
                i = i - 1;
            }
        }

        // starbase: block-built station, stationary in space
        if (sb_active != 0 && sb_z >= NEAR_Z)
        {
            float d;
            float sx;
            float sy;
            float s;
            d = sb_z;
            sx = CENTER_X + sb_x / d * FOCAL;
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

        // enemies (block-built Zylon cruisers), far to near
        i = 7;
        while (i >= 0)
        {
            if (enemies[i].alive != 0 && enemies[i].z >= NEAR_Z)
            {
                float d;
                float sx;
                float sy;
                float s;
                d = enemies[i].z;
                sx = CENTER_X + enemies[i].x / d * FOCAL;
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
            if (ebolts[i].active != 0 && ebolts[i].z >= NEAR_Z)
            {
                float d;
                float sx;
                float sy;
                float s;
                d = ebolts[i].z;
                sx = CENTER_X + ebolts[i].x / d * FOCAL;
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
                set_multiply_color( make_color_rgb( 120, 255, 180 ) );
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

        // explosions: expanding '*' at the kill position
        i = 0;
        while (i < 4)
        {
            if (explosions[i].t > 0)
            {
                float s;
                s = 1.0 + (20 - explosions[i].t) * 0.35;
                select_region( ASCII_STAR );
                set_multiply_color( make_color_rgb( 255, 200, 60 ) );
                draw_zoomed_centered( explosions[i].sx, explosions[i].sy, s );
            }
            i = i + 1;
        }

        draw_reticle();
        draw_hud();
        draw_radar();

        // damage flash: translucent red full-screen overlay
        if (flash_t > 0)
        {
            select_region( ASCII_DASH );
            set_multiply_color( make_color_rgba( 255, 40, 40, flash_t * 30 ) );
            set_drawing_scale( 64.0, 18.0 );
            draw_region_zoomed_at( 0, 0 );
            set_multiply_color( color_white );
        }

        // shield tint: light blue while up; flickers when damaged
        if (shields_on != 0)
        {
            int show;
            show = 1;
            if (shield_dmg != 0)
                show = (get_frame_counter() % 16) < 11;
            if (show != 0)
            {
                select_region( ASCII_DASH );
                set_multiply_color( make_color_rgba( 70, 130, 255, 34 ) );
                set_drawing_scale( 64.0, 18.0 );
                draw_region_zoomed_at( 0, 0 );
            }
            set_multiply_color( color_white );
        }

        // pause overlay
        if (pause_on != 0)
        {
            select_region( ASCII_DASH );
            set_multiply_color( make_color_rgba( 0, 0, 0, 170 ) );
            set_drawing_scale( 64.0, 18.0 );
            draw_region_zoomed_at( 0, 0 );
            set_multiply_color( color_white );
            print_at( CENTER_X - 30, CENTER_Y - 50, s_pause );
            print_at( CENTER_X - 80, CENTER_Y - 10, s_ph1 );
            print_at( CENTER_X - 80, CENTER_Y + 20, s_ph2 );
        }
    }

    // ------------------------------------------------------------------
    //  composite sprites, built from the block glyphs 0x11-0x14
    // ------------------------------------------------------------------

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
        select_region( ASCII_PLUS );
        set_multiply_color( make_color_rgb( 0, 200, 120 ) );
        draw_zoomed_centered( CENTER_X, CENTER_Y, 1.0 );
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
                if (ez < NEAR_Z)
                    ez = NEAR_Z;
                nx = asteroids[i].x / ez / 0.9;
                ny = 0 - asteroids[i].y / ez / 0.9;
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
                if (ez < NEAR_Z)
                    ez = NEAR_Z;
                nx = enemies[i].x / ez / 0.9;
                ny = 0 - enemies[i].y / ez / 0.9;
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
            i = i + 1;
        }

        // starbase
        if (sb_active != 0)
        {
            float nx;
            float ny;
            float ez;
            ez = sb_z;
            if (ez < NEAR_Z)
                ez = NEAR_Z;
            nx = sb_x / ez / 0.9;
            ny = 0 - sb_y / ez / 0.9;
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

    // galactic chart: 8x8 grid + ship status panel + cursor
    void draw_chart()
    {
        int gx;
        int gy;
        int cell_x;
        int cell_y;
        int cell;
        int hostile;

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
        if (cannons == 2)
            print_at( 8, 218, s_dmg_c2 );
        else if (cannons == 1)
            print_at( 8, 218, s_dmg_c1 );
        else
            print_at( 8, 218, s_dmg_c0 );
        strcpy( hud_line, s_dmg_e0 );
        if (engine_dmg == 0)
            strcat( hud_line, s_dmg_ok );
        else
            strcat( hud_line, s_dmg_bad );
        print_at( 8, 240, hud_line );
        if (shields_on != 0)
            print_at( 8, 262, s_sh_on );
        else
            print_at( 8, 262, s_sh_off );
        if (computer_on != 0)
            print_at( 8, 284, s_ac_on );
        else
            print_at( 8, 284, s_ac_off );
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

        // system status
        if (cannons == 2)
            print_at( 440, 104, s_dmg_c2 );
        else if (cannons == 1)
            print_at( 440, 104, s_dmg_c1 );
        else
            print_at( 440, 104, s_dmg_c0 );
        strcpy( hud_line, s_dmg_e0 );
        if (engine_dmg == 0)
            strcat( hud_line, s_dmg_ok );
        else
            strcat( hud_line, s_dmg_bad );
        print_at( 440, 128, hud_line );

        // engine speed bar
        print_at( 440, 152, s_spd );
        draw_bar( 440, 176, gear_frac(), 255, 200, 40 );

        // shields / attack computer state
        if (shields_on != 0)
            print_at( 440, 200, s_sh_on );
        else
            print_at( 440, 200, s_sh_off );
        if (computer_on != 0)
            print_at( 440, 224, s_ac_on );
        else
            print_at( 440, 224, s_ac_off );

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
            print_at( CENTER_X - 75, CENTER_Y - 10, s_repair );
        }
        else if (msg_t > 0)
        {
            print_at( CENTER_X - 55, CENTER_Y - 40, s_clear );
        }
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
    g_field.init( 3, 4 );

    while (1)
    {
        g_field.update();
        g_field.draw();
        end_frame();
    }
}
