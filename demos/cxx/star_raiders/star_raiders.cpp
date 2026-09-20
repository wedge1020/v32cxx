#title "Star Raiders"
#version 0.4

#include "video.h"
#include "input.h"
#include "math.h"
#include "string.h"
#include "time.h"

// ============================================================================
//  STAR RAIDERS for Vircon32
//  Phase 1: quadrant starfield (superseded)
//  Phase 2: Zylon enemies, phasers, energy/shields
//  Phase 3: galactic chart
//  Phase 4: FIRST-PERSON pseudo-3D space (like the original game)
//
//  The ship is at the center of a 3D relative space. Stars live at
//  (x, y, z) relative to the ship; D-pad rotates the view (yaw/pitch),
//  space always streams past (forward motion), and A engages warp.
//  Stars that pass behind the camera respawn at the far plane with a
//  new (x, y), so flying forever wraps around -- space feels spherical.
//  Quadrant tracking is kept for the galactic chart: our accumulated
//  yaw is quantized to a compass heading for warp jumps.
//
//  Written to the v32c++ subset: no templates, no static members, no
//  in-class initializers, no ternaries, no 'unsigned', one-word
//  parameter/return types. Tunables are #defines (Vircon C enforces
//  const strictly); text lives in ASCII-code int arrays (the v32c++
//  grammar has no string-literal array initializers).
//
//  Graphics use ONLY the BIOS font (texture -1, 10x20 glyphs, region
//  id = ASCII code). Glyph hotspots are left at the BIOS default
//  (top-left of each cell, as print_at expects); centered drawing is
//  done in screen space via draw_zoomed_centered instead, because
//  hotspot coordinates are ABSOLUTE texture positions and must not
//  be touched without knowing the font's cell layout.
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
#define ASCII_V     86    // 'V' -- enemy ship
#define ASCII_DASH  45    // '-' -- HUD bars
#define ASCII_STAR  42    // '*' -- explosions

// galaxy structure: 8x8 quadrants, each 8x8 sectors
#define GALAXY_QUADS       8
#define SECTORS_PER_QUAD   8
#define QUAD_SIZE   1024
#define QUAD_HALF    512

// 3D space
#define STAR_COUNT        140
#define NEAR_Z             50    // behind this = respawn
#define FAR_Z            1050
#define DEPTH_Z           990    // FAR_Z - NEAR_Z, respawn distance
#define FIELD_XY          700    // stars spawn within +-this in x/y
#define CRUISE_SPEED        6    // forward units per frame
#define WARP_SPEED         60
#define TURN_RATE        0.035    // radians per frame
#define FOCAL             300    // projection factor

#define WARP_FRAMES        90
#define PHASER_COST       1.5
#define WARP_COST         12.0
#define COLLISION_DAMAGE  25.0
#define ENERGY_REGEN      0.03
#define SHIELD_REGEN      0.02
#define BOLT_FRAMES         8
#define BOLT_STEP         140    // bolt z advance per frame
#define MSG_FRAMES        120

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
int s_warp[15]      = { 69, 78, 71, 73, 78, 69, 83, 58, 32, 87, 65, 82, 80, 0 };        // "ENGINES: WARP"
int s_cruise[17]    = { 69, 78, 71, 73, 78, 69, 83, 58, 32, 67, 82, 85, 73, 83, 69, 0 };// "ENGINES: CRUISE"
int s_off[13]       = { 69, 78, 71, 73, 78, 69, 83, 58, 32, 79, 70, 70, 0 };            // "ENGINES: OFF"
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

void hud_append_int( int v )
{
    itoa( v, hud_num, 10 );
    strcat( hud_line, hud_num );
}

// ---------------------------------------------------------------------------
//  Centered glyph drawing. BIOS font hotspots sit at each cell's TOP-LEFT
//  (print_at depends on that), and hotspot coordinates are ABSOLUTE
//  texture positions -- so we never touch hotspots. Instead we center
//  glyphs in screen space: a 10x20 cell drawn at scale s extends (10*s,
//  20*s) right/down from its drawing point, so we draw at (sx - 5*s,
//  sy - 10*s) to center it on (sx, sy).
// ---------------------------------------------------------------------------

void draw_zoomed_centered( int sx, int sy, float s )
{
    set_drawing_scale( s, s );
    draw_region_zoomed_at( sx - 5 * s, sy - 10 * s );
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
//  Star: position in the ship's relative 3D space
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

    // respawn at a uniformly random depth: placing stars at a fixed
    // distance (z + DEPTH) would bunch them into a thin shell that
    // whips past the camera in one frame at warp speed, leaving the
    // screen blank most of the time
    void respawn( RNG *rng )
    {
        x = rng->between( -FIELD_XY, FIELD_XY );
        y = rng->between( -FIELD_XY, FIELD_XY );
        z = NEAR_Z + 10 + rng->between( 0, DEPTH_Z - 20 );
    }
};

// ---------------------------------------------------------------------------
//  Enemy ship: also lives in relative 3D space, homes toward the ship
// ---------------------------------------------------------------------------

class Enemy
{
public:
    float x;
    float y;
    float z;
    int alive;
    int kind;      // 0 slow, 1 fast

    void randomize( RNG *rng )
    {
        x = rng->between( -FIELD_XY, FIELD_XY );
        y = rng->between( -FIELD_XY, FIELD_XY );
        z = rng->between( 400, FAR_Z );
        alive = 1;
        kind = rng->between( 0, 1 );
    }
};

// ---------------------------------------------------------------------------
//  Explosion: screen-space marker of a killed enemy
// ---------------------------------------------------------------------------

class Explosion
{
public:
    int sx;
    int sy;
    int t;      // frames remaining
};

// ---------------------------------------------------------------------------
//  The game
// ---------------------------------------------------------------------------

class Starfield
{
public:
    Star stars[140];
    Enemy enemies[8];
    Explosion explosions[4];
    RNG rng;

    int qx;
    int qy;
    float shipx;        // quadrant-space position (for the HUD)
    float shipy;
    float yaw;          // accumulated view yaw (0 = north)
    float pitch;        // accumulated view pitch (for the nav readout)
    int warp_t;
    int warp_dir;

    float energy;
    float shields;
    int bolt_t;         // phaser bolt in flight
    float bolt_z;       // bolt distance along the view axis
    int kills;
    int msg_t;
    int dead;

    int chart_on;
    int prev_y;
    int galaxy_map[64];
    int cleared[64];

    // ------------------------------------------------------------------

    // deterministic enemy count for a quadrant we have NOT entered
    int quadrant_enemies( int cqx, int cqy )
    {
        int i;
        int count;
        rng.seed( (cqy * GALAXY_QUADS + cqx + 1) * 7919 );
        i = 0;
        while (i < STAR_COUNT * 4)
        {
            rng.next();
            i = i + 1;
        }
        count = 0;
        if (rng.between( 0, 3 ) < 3)
            count = rng.between( 1, 5 );
        return count;
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
        i = 0;
        while (i < 4)
        {
            explosions[i].t = 0;
            i = i + 1;
        }
        msg_t = 0;
    }

    void init( int start_qx, int start_qy )
    {
        int i;
        yaw = 0;
        pitch = 0;
        warp_t = 0;
        warp_dir = 0;
        shipx = QUAD_HALF;
        shipy = QUAD_HALF;
        energy = 100;
        shields = 100;
        bolt_t = 0;
        kills = 0;
        dead = 0;
        msg_t = 0;
        chart_on = 0;
        prev_y = 0;
        i = 0;
        while (i < GALAXY_QUADS * GALAXY_QUADS)
        {
            galaxy_map[i] = quadrant_enemies( i % GALAXY_QUADS, i / GALAXY_QUADS );
            cleared[i] = 0;
            i = i + 1;
        }
        enter_quadrant( start_qx, start_qy );
    }

    // compass heading (0=N 1=E 2=S 3=W) quantized from the yaw
    int heading()
    {
        int h;
        float a;
        a = yaw / 1.5707963;      // yaw / (pi/2)
        h = a;
        if (a - h >= 0.5)
            h = h + 1;
        h = h % 4;
        if (h < 0)
            h = h + 4;
        return h;
    }

    // heading in degrees, 0 = north, growing clockwise (east = 90)
    int nav_heading_deg()
    {
        int d;
        d = yaw * 57.29578;
        d = d % 360;
        if (d < 0)
            d = d + 360;
        return d;
    }

    // pitch in degrees, negative = nose up
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

    // ------------------------------------------------------------------

    void update()
    {
        float turn;
        float speed;
        int i;

        if (dead != 0)
        {
            if (gamepad_button_start() > 0)
                init( 3, 4 );
            return;
        }

        // Y toggles the galactic chart (rising edge only)
        if (gamepad_button_y() > 0 && prev_y <= 0)
            chart_on = 1 - chart_on;
        prev_y = gamepad_button_y();

        // while the chart is up, the game is paused
        if (chart_on != 0)
            return;

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
        // clamp displayed pitch to +-35 degrees
        if (pitch > 0.61)
            pitch = 0.61;
        if (pitch < -0.61)
            pitch = -0.61;

        // forward speed: cruising always; warp on A
        speed = CRUISE_SPEED;
        if (warp_t == 0)
        {
            if (gamepad_button_a() > 0 && energy >= WARP_COST)
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
                arrive();
            }
        }

        // move the ship through quadrant space along its facing
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

        // stars stream past
        i = 0;
        while (i < STAR_COUNT)
        {
            stars[i].z = stars[i].z - speed;
            if (stars[i].z < NEAR_Z)
                stars[i].respawn( &rng );
            i = i + 1;
        }

        // phaser bolt: X fires along the view axis
        if (bolt_t > 0)
        {
            bolt_t = bolt_t - 1;
            bolt_z = bolt_z + BOLT_STEP;
        }
        if (gamepad_button_x() > 0 && bolt_t == 0 && energy >= PHASER_COST)
        {
            energy = energy - PHASER_COST;
            bolt_t = BOLT_FRAMES;
            bolt_z = NEAR_Z + 30;
            fire_phaser();
        }

        if (warp_t == 0)
            update_enemies();

        // explosions decay
        i = 0;
        while (i < 4)
        {
            if (explosions[i].t > 0)
                explosions[i].t = explosions[i].t - 1;
            i = i + 1;
        }

        // regeneration
        energy = energy + ENERGY_REGEN;
        if (energy > 100)
            energy = 100;
        shields = shields + SHIELD_REGEN;
        if (shields > 100)
            shields = 100;

        if (msg_t > 0)
            msg_t = msg_t - 1;
    }

    // rotate all stars and enemies around the Y axis (camera yaw).
    // turning right (d>0) moves dead-ahead points to the LEFT on screen.
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
            if (stars[i].z < NEAR_Z)
                stars[i].respawn( &rng );
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
    }

    // rotate around the X axis (camera pitch).
    // nose up (d>0) moves dead-ahead points DOWN on screen.
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
            if (stars[i].z < NEAR_Z)
                stars[i].respawn( &rng );
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
    }

    // phaser hit test: nearest live enemy inside the view cone
    // (|x| and |y| small relative to z, in front of us)
    void fire_phaser()
    {
        int i;
        int best;
        float bestd;
        best = -1;
        bestd = 0;
        i = 0;
        while (i < 8)
        {
            if (enemies[i].alive != 0 && enemies[i].z > NEAR_Z)
            {
                float limx;
                float limy;
                float d;
                limx = enemies[i].z * 0.30;
                limy = enemies[i].z * 0.30;
                d = enemies[i].z;
                if (enemies[i].x > -limx && enemies[i].x < limx &&
                    enemies[i].y > -limy && enemies[i].y < limy)
                {
                    if (best == -1 || d < bestd)
                    {
                        best = i;
                        bestd = d;
                    }
                }
            }
            i = i + 1;
        }
        if (best != -1)
        {
            // remember where it was, for the explosion marker
            add_explosion( best );
            enemies[best].alive = 0;
            kills = kills + 1;
            if (enemies_alive() == 0)
            {
                cleared[qy * GALAXY_QUADS + qx] = 1;
                msg_t = MSG_FRAMES;
            }
        }
    }

    void add_explosion( int i )
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
        d = enemies[i].z;
        if (d < NEAR_Z)
            d = NEAR_Z;
        explosions[slot].sx = CENTER_X + enemies[i].x / d * FOCAL;
        explosions[slot].sy = CENTER_Y - enemies[i].y / d * FOCAL;
        explosions[slot].t = 20;
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
                // home toward the ship at the origin
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
                    shields = shields - COLLISION_DAMAGE;
                    if (shields <= 0)
                    {
                        shields = 0;
                        dead = 1;
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

        // star size boost with speed: subtle at cruise, big at warp
        speed = CRUISE_SPEED;
        if (warp_t > 0)
            speed = WARP_SPEED;
        speedscale = speed / CRUISE_SPEED;

        clear_screen( make_color_rgb( 2, 4, 12 ) );

        // stars, far to near
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
                    b = 255 - d * 0.19;      // brighter when closer
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

        // enemies (draw far to near so closer ones overlap)
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
                if (s < 0.7)
                    s = 0.7;
                if (s > 8.0)
                    s = 8.0;
                if (sx > -40 && sx < 680 && sy > -40 && sy < 400)
                {
                    select_region( ASCII_V );
                    if (enemies[i].kind == 1)
                        set_multiply_color( make_color_rgb( 255, 80, 60 ) );
                    else
                        set_multiply_color( make_color_rgb( 255, 170, 40 ) );
                    draw_zoomed_centered( sx, sy, s );
                }
            }
            i = i - 1;
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

        // phaser bolts: twin bolts fired from the ship's wing roots
        // (wide apart, below the view center), converging slowly
        // toward the view axis as they fly. Drawn as zoomed '*'.
        if (bolt_t > 0)
        {
            float s;
            float offs;
            s = 1400 / bolt_z;
            if (s < 1.5)
                s = 1.5;
            offs = 250 - bolt_z * 0.22;   // 250px apart at launch, ~0 at 1100
            if (offs < 0)
                offs = 0;
            select_region( ASCII_STAR );
            set_multiply_color( make_color_rgb( 120, 255, 180 ) );
            draw_zoomed_centered( CENTER_X - offs, CENTER_Y + 40, s );
            draw_zoomed_centered( CENTER_X + offs, CENTER_Y + 40, s );
        }

        draw_reticle();
        draw_hud();
    }

    void draw_reticle()
    {
        select_region( ASCII_PLUS );
        set_multiply_color( make_color_rgb( 0, 200, 120 ) );
        draw_zoomed_centered( CENTER_X, CENTER_Y, 1.0 );
    }

    // horizontal bar: the '-' glyph zoomed non-uniformly. The glyph's
    // horizontal line sits at the vertical center of its 10x20 cell,
    // so a bar anchored at (x, y) with scale (20*f, 0.5) spans
    // x..x+200*f horizontally and lands on y+5 vertically.
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

    // galactic chart: 8x8 grid, one cell per quadrant
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
        print_at( 260, 320, s_yclose );

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
                }
                else if (hostile > 0)
                {
                    select_region( ASCII_V );
                    set_multiply_color( make_color_rgb( 255, 90, 70 ) );
                }
                else
                {
                    select_region( ASCII_DOT );
                    set_multiply_color( make_color_rgb( 90, 90, 110 ) );
                }
                draw_zoomed_centered( cell_x, cell_y, 1.0 );
                cell_x = cell_x + 20;
            }
            cell_x = 240;
            cell_y = cell_y + 25;
        }
    }

    void draw_hud()
    {
        set_multiply_color( color_white );

        // top-left: position + enemy count
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

        // navigation: heading (0-359) and pitch (degrees), plus raw
        // quadrant-space XY position
        strcpy( hud_line, s_hdg );
        hud_append_int( nav_heading_deg() );
        strcat( hud_line, s_pit );
        hud_append_int( nav_pitch_deg() );
        strcat( hud_line, s_pos );
        hud_append_int( shipx );
        strcat( hud_line, s_comma );
        hud_append_int( shipy );
        print_at( 8, 56, hud_line );

        // top-right: energy and shield bars
        print_at( 440, 8, s_energy );
        draw_bar( 440, 32, energy / 100, 80, 220, 80 );
        print_at( 440, 56, s_shields );
        draw_bar( 440, 80, shields / 100, 80, 140, 255 );

        // bottom-left: engines + heading
        if (warp_t > 0)
            strcpy( hud_line, s_warp );
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

        // center messages
        if (dead != 0)
        {
            print_at( CENTER_X - 60, CENTER_Y - 40, s_destroyed );
            print_at( CENTER_X - 50, CENTER_Y - 10, s_restart );
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
