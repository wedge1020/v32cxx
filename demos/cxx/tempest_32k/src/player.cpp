// *****************************************************************************
//  TEMPEST 32K -- src/player.cpp
//  firing, the superzapper, claw movement and jumping, the CPU claw, hits
// *****************************************************************************
#include "tempest.hpp"

// ---------------------------------------------------------------------------
//  Input / update
// ---------------------------------------------------------------------------
void kill_player( G* g );
void level_clear( G* g );

// firing, factored per player: owner 0 = P1 (pad 0), 1 = P2 (pad 1).
// The laser / rapid timers are SHARED in co-op — pods power the ship,
// not one gunner — but each player keeps their own cooldown. Every
// bullet is stamped with its owner so kills score to the right player.
void fire_owner( G* g, int owner, float lane )
{
    if( owner == 0 && g->fire_cooldown > 0 ) return;
    if( owner == 1 && g->p2_fire_cooldown > 0 ) return;
    sfx( g, PEWPEW );
    // SUPER LASER: three shots at once — the claw's own lane plus the
    // two half-lanes either side of it, blanketing neighbouring rim
    // lanes. Slightly slower cycle than the single blaster.
    if( g->laser_timer > 0 )
    {
        if( owner == 0 ) g->fire_cooldown = 8;
        else             g->p2_fire_cooldown = 8;
        int k;
        for( k = -1; k <= 1; k++ )
        {
            float ln = lane + k * 1.5;
            if( ln < 0 ) ln += LANES;
            if( ln >= LANES ) ln -= LANES;
            int i;
            for( i = 0; i < MAX_BULLETS; i++ )
            {
                if( !g->BULLETS[ i ].alive )
                {
                    g->BULLETS[ i ].alive = 1;
                    g->BULLETS[ i ].owner = owner;
                    g->BULLETS[ i ].lane = ln;
                    g->BULLETS[ i ].z = 0.02;
                    break;
                }
            }
        }
        return;
    }
    // RAPID BLASTER: 25% faster cycle -- alternating 5/4-frame
    // cooldowns average 4.5 (the cooldown is an int, the rate isn't).
    int cd = g->rapid_timer > 0 ? ( ( g->frame & 1 ) ? 5 : 4 ) : 6;
    if( owner == 0 ) g->fire_cooldown = cd;
    else             g->p2_fire_cooldown = cd;
    int i;
    for( i = 0; i < MAX_BULLETS; i++ )
    {
        if( !g->BULLETS[ i ].alive )
        {
            g->BULLETS[ i ].alive = 1;
            g->BULLETS[ i ].owner = owner;
            g->BULLETS[ i ].lane = lane;
            g->BULLETS[ i ].z = 0.02;
            return;
        }
    }
}

void fire( G* g )
{
    fire_owner( g, 0, g->player_lane );
}

void fire2( G* g )
{
    fire_owner( g, 1, g->p2_lane );
}

void superzap( G* g, int owner )
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
            if( owner == 0 ) g->score += 150;
            else             g->p2_score += 150;
        }
    }
    // the zap can wipe out the last enemies — check for level clear
    if( g->spawn_timer > 999 && count_enemies( g ) == 0 ) level_clear( g );
}

// claw movement, factored out of update_player so it can also run
// during the warp-out transition (dodge the spikes streaming past!).
// LANE-VERSION: the moved lane comes in by POINTER (scalar references
// are a documented transpiler gap), so P2's claw shares the exact
// same movement + rim-clamp code. airborne = 1 suspends the open-web
// rim clamp.
void clamp_claw_lane( G* g, float* lane, int airborne );

void move_claw_lane( G* g, float* lane, int airborne )
{
    if( gamepad_left() > 0 )  *lane -= 0.12;
    if( gamepad_right() > 0 ) *lane += 0.12;
    clamp_claw_lane( g, lane, airborne );
}

// wrap the lane, then keep the claw on its own rim run (shared by the
// human and CPU claws -- the CPU claw used to skip the run clamp and
// could drift into a gap)
void clamp_claw_lane( G* g, float* lane, int airborne )
{
    if( *lane < 0 ) *lane += LANES;
    if( *lane >= LANES ) *lane -= LANES;

    // OPEN WEBS: clamp the claw to its contiguous rim run so it can't
    // slide over a missing outline edge (see run_bounds). Fully closed
    // webs skip the clamp entirely — free movement all the way around.
    // While AIRBORNE (jump) the clamp is suspended — leaping a rim gap
    // is the one way to cross between a web's separated arcs.
    // The run is looked up from the NEAREST lane, not the truncated
    // one: a claw at 8.91 sits in the (8,9) gap and must fall back to
    // lane 9's run — with truncation it clamped into the OTHER arc
    // (the cross-half teleport bug).
    if( !web_full( g ) && !airborne )
    {
        int plo; int phi;
        int pli = (int)( *lane + 0.5 );
        if( pli >= LANES ) pli -= LANES;
        run_bounds( g, pli, &plo, &phi );
        if( plo == phi ) *lane = plo;               // single-lane run
        else if( plo < phi )
        {
            if( *lane < plo ) *lane = plo;
            if( *lane > phi ) *lane = phi;
        }
        else  // wrapped run [plo..15]+[0..phi]: the gap zone is the
              // open interval (phi,plo) — ONLY positions there get
              // clamped; everything else (including the 15->0 seam
              // crossing) is legal run space. Snap back to whichever
              // END of the gap the claw just left (nearest by run
              // midpoint) — never across it.
        {
            if( *lane > phi && *lane < plo )
            {
                float mid = ( plo + phi ) * 0.5;
                if( *lane < mid ) *lane = phi;
                else             *lane = plo;
            }
        }
    }
}

void move_claw( G* g )
{
    move_claw_lane( g, &g->player_lane, g->jump_timer > 0 );

    // camera sway: ease the vanishing point toward a fraction of the
    // claw's offset from the tube axis — the tube appears to lean and
    // follow you around the web (Tempest 2000 flavour). P1 drives the
    // camera in co-op; P2's claw does not lean the tube.
    project( g, g->player_lane, 0 );
    float camtx = ( g->px - CX ) * CAM_SWAY;
    float camty = ( g->py - CY ) * CAM_SWAY;
    g->cam_x += ( camtx - g->cam_x ) * CAM_EASE;
    g->cam_y += ( camty - g->cam_y ) * CAM_EASE;
}

// P2's claw: identical movement and rim clamp, no camera sway
void move_claw2( G* g )
{
    move_claw_lane( g, &g->p2_lane, g->p2_jump_timer > 0 );
}

// JUMP (button Y), factored out so it also runs during the warp-out:
// the claw leaps off the web toward the camera — airborne frames are
// invulnerable at the rim, so you can hop over a camper, or over a
// spike sweeping past during the level transition. The timers come in
// by pointer so both players share the one code path.
void claw_jump_lane( G* g, float* lane, int* jt, int* jc )
{
    if( *jc > 0 ) ( *jc )--;
    if( *jt > 0 ) ( *jt )--;
    else if( gamepad_button_y() == 1 && *jc == 0 )
    {
        *jt = 34;
        *jc = 60;   // lands at 34, 26 frames of recovery
        project( g, *lane, 0 );
        burst( g, g->px, g->py, 8, 1.5 );   // takeoff puff
        sfx( g, JUMP );
    }
}

void claw_jump( G* g )
{
    claw_jump_lane( g, &g->player_lane, &g->jump_timer, &g->jump_cooldown );
}

void claw_jump2( G* g )
{
    claw_jump_lane( g, &g->p2_lane, &g->p2_jump_timer, &g->p2_jump_cooldown );
}

// is there a physical gamepad on port n? gamepad_is_connected()
// reads the CURRENTLY SELECTED pad — so select, read, and restore
// pad 0 (the pad every menu reads).
int pad_connected( int n )
{
    select_gamepad( n );
    int c = 0;
    if( gamepad_is_connected() ) c = 1;
    select_gamepad( 0 );
    return c;
}

// a player claw with NO gamepad on its port is CPU-driven: a
// watered-down AI buddy. It hunts the enemy nearest the rim, but
// moves AND fires slower than the drone power-up, and scales
// INVERSELY with difficulty — fastest on easy, slowest on hard.
// It never jumps, never superzaps, and it parks OUTSIDE the rim
// contact kill zone (0.7 lanes) so it shoots campers instead of
// walking into them.
// 1 when lane l lies on the rim run [lo..hi] (which may wrap past 15)
int lane_in_run( float l, int lo, int hi )
{
    if( lo <= hi ) return l >= lo - 0.01 && l <= hi + 0.01;
    return l >= lo - 0.01 || l <= hi + 0.01;
}

void ai_claw( G* g, int who )
{
    float* lane = who == 1 ? &g->p2_lane : &g->player_lane;
    int* fcd = who == 1 ? &g->p2_fire_cooldown : &g->fire_cooldown;
    int* jt = who == 1 ? &g->p2_jump_timer : &g->jump_timer;
    int* jc = who == 1 ? &g->p2_jump_cooldown : &g->jump_cooldown;

    // a human's jump timers tick in claw_jump_lane; the CPU's tick here
    if( *jc > 0 ) ( *jc )--;
    if( *jt > 0 ) ( *jt )--;

    // REACHABLE targets only: on an open web the claw can't cross a rim
    // gap, so an enemy on another arc is not a target -- chasing one
    // (the old rule) parked the claw against the gap forever once the
    // nearest-to-rim enemy happened to sit across it, and the level
    // never ended.
    int lo = 0;
    int hi = LANES - 1;
    int open = !web_full( g );
    if( open ) run_bounds( g, ( (int)( *lane + 0.5 ) ) & ( LANES - 1 ), &lo, &hi );

    // target: the reachable enemy closest to the rim (the imminent
    // threat). No reachable enemies = hold position.
    int best = -1;
    float bestz = 1.1;
    int other = -1;           // nearest enemy on ANOTHER arc, by lane distance
    float otherd = 99;
    int i;
    for( i = 0; i < MAX_ENEMIES; i++ )
    {
        Enemy* e = &g->ENEMIES[ i ];
        if( !e->alive ) continue;
        if( open && !lane_in_run( e->lane, lo, hi ) )
        {
            float od = e->lane - *lane;
            while( od > LANES / 2 ) od -= LANES;
            while( od < -LANES / 2 ) od += LANES;
            if( od < 0 ) od = -od;
            if( od < otherd ) { otherd = od; other = i; }
            continue;
        }
        if( e->z < bestz ) { bestz = e->z; best = i; }
    }
    if( best < 0 && other >= 0 )
    {
        // EVERY enemy is on another arc: head for it and LEAP the gap
        // (the clamp is suspended while airborne). Blocked against the
        // run's end = time to jump.
        float d = g->ENEMIES[ other ].lane - *lane;
        while( d > LANES / 2 ) d -= LANES;
        while( d < -LANES / 2 ) d += LANES;
        float before = *lane;
        float step = d > 0 ? 0.12 : -0.12;          // a human claw's top speed
        if( *jt > 0 || ( d < 0.12 && d > -0.12 ) ) step = d * 0.15;
        *lane += step;
        clamp_claw_lane( g, lane, *jt > 0 );
        float moved = *lane - before;
        if( moved < 0 ) moved = -moved;
        if( *jt == 0 && *jc == 0 && moved < 0.001 )
        {
            *jt = 34;
            *jc = 60;
            project( g, *lane, 0 );
            burst( g, g->px, g->py, 8, 1.5 );
            sfx( g, JUMP );
        }
        return;
    }
    if( best >= 0 )
    {
        Enemy* e = &g->ENEMIES[ best ];
        float d = e->lane - *lane;
        while( d > LANES / 2 ) d -= LANES;
        while( d < -LANES / 2 ) d += LANES;
        float ad = d < 0 ? -d : d;
        float ease = g->difficulty == 0 ? 0.06 : g->difficulty == 2 ? 0.03 : 0.045;
        int acd = g->difficulty == 0 ? 70 : g->difficulty == 2 ? 120 : 95;
        // a spiker hides behind its own spike, which eats shots (each
        // trims it 0.10) and regrows 0.006 a frame: at the usual pace
        // the spike wins forever. Chip through it at ~2/3 human pace.
        if( e->type == 2 ) acd = 10;
        // RIM CAMPER: it kills anything within 0.7 lanes, but a shot only
        // connects within 0.6 -- the one way to kill it is the JUMP-SHOT
        // (an airborne claw is invulnerable). Wait just outside the kill
        // zone with the gun ready, leap, slide into line, fire.
        int camper = e->z < 0.05 && e->type != 2;
        int ready = *jc == 0 && *fcd <= 0;      // jump AND gun available
        if( camper && *jt > 0 )
        {
            *lane += d * 0.15;                       // airborne: close in
            if( ad < 0.55 && *fcd <= 0 ) { fire_owner( g, who, *lane ); *fcd = acd; }
        }
        else if( camper && ( ( ad >= 0.9 && ad <= 1.2 ) || ( ad < 0.9 && ready ) ) )
        {
            *jt = 34;                                // the same leap as claw_jump_lane
            *jc = 60;
            project( g, *lane, 0 );
            burst( g, g->px, g->py, 8, 1.5 );
            sfx( g, JUMP );
        }
        else if( camper && ad < 0.9 )
        {
            // too close and not ready to leap: step away -- toward
            // whichever side the run allows (d may be 0: sitting right
            // on it, e.g. clamped at a run end where it camps)
            float away = d > 0 ? -0.08 : 0.08;
            float before = *lane;
            *lane += away;
            clamp_claw_lane( g, lane, 0 );
            if( *lane == before ) *lane -= away * 2;
        }
        else if( camper )
            *lane += d * ease;                       // approach the waiting spot
        else
        {
            // a CLIMBING enemy is harmless until it reaches the rim: line
            // up under it and shoot. Close to the rim, park about a lane
            // off instead and let it come into the line of fire (slower
            // than the buddy drone's 0.08 ease on every difficulty).
            float stand = e->z > 0.15 ? 0.3 : 1.0;
            if( ad > stand ) *lane += d * ease;
            // fire when roughly aligned -- long cooldowns (the drone runs
            // 45, or 80 on easy), again inverse with difficulty
            if( ad < 0.55 && *fcd <= 0 ) { fire_owner( g, who, *lane ); *fcd = acd; }
        }
    }
    clamp_claw_lane( g, lane, *jt > 0 );
}

void update_player( G* g )
{
    // the SHARED pod timers tick even when P1 is out of the game,
    // so P2 keeps the laser / rapid power running in co-op
    if( g->laser_timer > 0 ) g->laser_timer--;
    if( g->rapid_timer > 0 ) g->rapid_timer--;
    if( g->p1_out ) return;
    if( g->p1_respawn > 0 ) g->p1_respawn--;
    // no pad on port 0 = CPU-driven P1 (p1_cpu is sampled once per
    // frame in main)
    if( g->p1_cpu ) ai_claw( g, 0 );
    else
    {
        move_claw( g );
        claw_jump( g );
        if( gamepad_button_a() > 0 ) fire( g );
        if( gamepad_button_b() == 1 ) superzap( g, 0 );   // == 1: just-pressed edge
    }
    if( g->fire_cooldown > 0 ) g->fire_cooldown--;
}

// PLAYER 2 (co-op): manages its OWN pad selection. The shared
// laser/rapid timers are NOT touched here (update_player already
// ticks them exactly once per frame). Human P2 can pause the game;
// CPU P2 (no pad on port 1) cannot.
void update_player2( G* g )
{
    if( g->p2_out ) return;
    if( g->p2_respawn > 0 ) g->p2_respawn--;
    if( g->p2_cpu ) ai_claw( g, 1 );
    else
    {
        select_gamepad( 1 );
        move_claw2( g );
        claw_jump2( g );
        if( gamepad_button_a() > 0 ) fire2( g );
        if( gamepad_button_b() == 1 ) superzap( g, 1 );
        if( gamepad_button_start() == 1 ) g->state = 5;
        select_gamepad( 0 );
    }
    if( g->p2_fire_cooldown > 0 ) g->p2_fire_cooldown--;
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

// both players out of lives: end the run. Only HUMAN players' scores
// may reach the high-score table — a CPU-driven claw (no gamepad on
// its port) cannot post an entry — so the submitted team result is
// the best score among CONNECTED players only. All-human runs submit
// the higher tally; a CPU-assisted run submits the human's score; no
// humans at all submits nothing.
void coop_game_over( G* g )
{
    int best = -1;
    if( !g->p1_cpu ) best = g->score;
    if( !g->p2_cpu )
    {
        if( best < 0 ) best = g->p2_score;
        else if( g->p2_score > best ) best = g->p2_score;
    }
    if( best < 0 ) g->score = 0;
    else           g->score = best;
    g->state = 3;
    show_message( g, "GAME OVER" );
    g->state_timer = 300;
    play_title_music( g );   // gameplay succession ends here
}

// who got hit: 0 = P1, 1 = P2. SOLO is unchanged — kill_player's full
// death cinematic and the restart-via-state-1 flow stay exactly as
// they were. In CO-OP a hit costs the struck player one life: they
// blink invulnerable for 2 seconds while respawning in place. The
// run ends as soon as no HUMAN players remain — a lone CPU survivor
// never carries the game on alone: it cannot post a high score, so
// letting it play on (as an early build did) just stalls the
// GAME OVER screen behind minutes of CPU play the user cannot
// influence. Two humans still get the full both-out rule.
void hit_player( G* g, int who )
{
    if( !g->twoplayer )
    {
        if( who == 0 ) kill_player( g );
        return;
    }
    if( who == 0 )
    {
        if( g->p1_out || g->p1_respawn > 0 ) return;
        g->lives--;
        project( g, g->player_lane, 0 );
        explode( g, g->px, g->py, 40, 4 );
        sfx( g, DEATH );
        if( g->lives <= 0 )
        {
            g->p1_out = 1;
            show_message( g, "P1 OUT!" );
            // no human left the moment P2 is out OR CPU-driven
            if( g->p2_out || g->p2_cpu ) coop_game_over( g );
        }
        else
        {
            g->p1_respawn = 120;
            show_message( g, "P1 DOWN!" );
        }
    }
    else
    {
        if( g->p2_out || g->p2_respawn > 0 ) return;
        g->p2_lives--;
        project( g, g->p2_lane, 0 );
        explode( g, g->px, g->py, 40, 4 );
        sfx( g, DEATH );
        if( g->p2_lives <= 0 )
        {
            g->p2_out = 1;
            show_message( g, "P2 OUT!" );
            // no human left the moment P1 is out OR CPU-driven
            if( g->p1_out || g->p1_cpu ) coop_game_over( g );
        }
        else
        {
            g->p2_respawn = 120;
            show_message( g, "P2 DOWN!" );
        }
    }
}

void level_clear( G* g );
