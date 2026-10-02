// *****************************************************************************
//  TEMPEST 32K -- src/powerups.cpp
//  power-up pods and the AI buddy drone
// *****************************************************************************
#include "tempest.hpp"

// ---------------------------------------------------------------------------
//  Power-ups: shoot the pods to collect them.
//    type 0 = SUPERZAP refill, type 1 = AI buddy, type 2 = extra life,
//    type 3 = SUPER LASER (triple-shot for a timed stretch),
//    type 4 = RAPID BLASTER (25% faster single shots; easy = unlimited)
// ---------------------------------------------------------------------------
void collect_powerup( G* g, int i, int owner )
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
            if( owner == 0 ) g->score += 250;
            else             g->p2_score += 250;
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
        // extra life (or bonus, at the cap) goes to the SHOOTING
        // player — the rest of the pod roster is team-shared
        int lv = g->lives;
        if( owner == 1 ) lv = g->p2_lives;
        if( lv < 4 )
        {
            if( owner == 0 ) g->lives++;
            else             g->p2_lives++;
            show_message( g, "EXTRA LIFE!" );
        }
        else
        {
            if( owner == 0 ) g->score += 250;
            else             g->p2_score += 250;
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
    for( i = 0; i < MAX_POWERUPS; i++ )
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
    for( i = 0; i < MAX_POWERUPS; i++ )
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
        for( i = 0; i < MAX_POWERUPS; i++ ) if( g->POWERUPS[ i ].alive ) n++;
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
    // enemies alive, fall back to hovering beside the claw. Ease
    // factor lowered 0.2 -> 0.08: at 0.2 the drone SNAPPED onto a
    // target lane (up to 1.6 lanes/frame — 13x the claw!) and beat
    // the player to everything they aimed at. It now glides visibly
    // and can be outrun.
    float want = g->player_lane + 1.9;
    if( best >= 0 ) want = g->ENEMIES[ best ].lane;
    float d = want - g->buddy_lane;
    while( d > LANES / 2 ) d -= LANES;
    while( d < -LANES / 2 ) d += LANES;
    g->buddy_lane += d * 0.08;
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
                    g->BULLETS[ j ].owner = 0;   // drone kills score P1
                    g->BULLETS[ j ].lane = g->buddy_lane;
                    g->BULLETS[ j ].z = 0.10;
                    // cooldown lengthened so the drone SUPPORTS the
                    // player instead of playing for them; on easy it
                    // is a background helper, not a second gun
                    int bcd = 45;
                    if( g->difficulty == 0 ) bcd = 80;
                    g->buddy_cooldown = bcd;
                    break;
                }
            }
        }
    }
}
