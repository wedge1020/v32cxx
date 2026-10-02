// *****************************************************************************
//  TEMPEST 32K -- src/world.cpp
//  messages, enemy spawning, particle bursts, shockwaves
// *****************************************************************************
#include "tempest.hpp"

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
            g->ENEMIES[ i ].wig = rng( g ) % 256;   // worm: random cycle phase
            g->ENEMIES[ i ].dir = ( rng( g ) % 2 ) * 2 - 1;
            g->ENEMIES[ i ].stretch = 0;           // worm: starts bunched
            g->ENEMIES[ i ].segs = 1;
            if( type == 4 )
                g->ENEMIES[ i ].segs = WORM_MIN_SEGMENTS
                    + rng( g ) % ( WORM_MAX_SEGMENTS - WORM_MIN_SEGMENTS + 1 );
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
            g->ENEMIES[ i ].stretch = 0;
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

int count_worms( G* g )
{
    int i; int n = 0;
    for( i = 0; i < MAX_ENEMIES; i++ )
        if( g->ENEMIES[ i ].alive && g->ENEMIES[ i ].type == 4 ) n++;
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
    // g->partcount (maintained once per frame in main) replaces a
    // fresh 220-slot walk per explosion — one frame stale is fine
    // for a throttle heuristic, and a superzap chain no longer pays
    // a recount per enemy.
    int alivep = g->partcount;
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
    for( s = 0; s < MAX_SHOCKS; s++ )
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
