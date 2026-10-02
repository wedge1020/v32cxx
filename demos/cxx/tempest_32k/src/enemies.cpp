// *****************************************************************************
//  TEMPEST 32K -- src/enemies.cpp
//  enemy behaviour, bullets and their collisions, level clear
// *****************************************************************************
#include "tempest.hpp"

void update_enemies( G* g )
{
    int i;
    // SPEED RAMP, RESCALED ACROSS THE BOARD: player feedback —
    // even medium was unmanageable past the mid levels, not from
    // enemy COUNT but from ADVANCE SPEED: enemies hit the rim
    // faster than the claw could circle the web, and (with the
    // bullet-order bug) rim campers could not be shot at all.
    // Base curve lowered ~20%, past-16 ramp kept at 35%, and the
    // difficulty spread flattened so every level buys the player
    // a real beat to reposition. Cross times at MEDIUM, rim to
    // far cap: ~2.6s at level 17, ~2.3s at level 32.
    // HOISTED out of the loop: identical for every enemy, it was
    // recomputed per enemy per frame for nothing.
    float lv = g->level;
    if( lv > 16 ) lv = 16 + ( lv - 16 ) * 0.35;
    float speed = 0.0028 + lv * 0.0003;
    if( g->difficulty == 0 ) speed *= 0.55;
    if( g->difficulty == 1 ) speed *= 0.80;
    if( g->difficulty == 2 ) speed *= 1.05;
    for( i = 0; i < MAX_ENEMIES; i++ )
    {
        if( !g->ENEMIES[ i ].alive ) continue;

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
                if( diffw < 0.7 && g->jump_timer <= 0 ) hit_player( g, 0 );
                // P2 gets their own lane/jump test on the shared web
                if( g->twoplayer && !g->p2_out )
                {
                    float d2w = g->p2_lane - g->ENEMIES[ i ].lane;
                    if( d2w < 0 ) d2w = -d2w;
                    if( d2w > LANES / 2 ) d2w = LANES - d2w;
                    if( d2w < 0.7 && g->p2_jump_timer <= 0 ) hit_player( g, 1 );
                }

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

        // INCHWORM (type 4, 'W' electric): TRUE inchworm mechanics —
        // the body is a SPAN of the strand, and its two ends move
        // ALTERNATELY, never together. z is the HEAD, stretch the
        // body's z-length trailing behind it. SURGE (phase 0..31):
        // the tail stays anchored while the head surges forward —
        // the worm ADVANCES exactly as it elongates. CATCH-UP
        // (32..63): the head pauses while the tail slides forward
        // and the body shrinks back to a point. Net advance per
        // 64-frame cycle = reach = speed*64, so the AVERAGE climb
        // speed equals a flipper's — but the motion is pulses:
        // surge, stall, surge. (An earlier draft just sped up and
        // slowed down a pulsating sprite: it read as a throbbing
        // blob sliding at even pace — the stretch has to move the
        // WORM, not just resize it.)
        if( g->ENEMIES[ i ].type == 4 )
        {
            g->ENEMIES[ i ].wig += 3;
            float reach = speed * 64.0;
            int ph = g->ENEMIES[ i ].wig & 63;
            float step = reach * 0.03125;      // reach/32 per frame
            if( ph < 32 )
            {
                g->ENEMIES[ i ].z -= step;         // head surges ahead
                g->ENEMIES[ i ].stretch += step;   // body elongates
            }
            else
            {
                g->ENEMIES[ i ].stretch -= step;   // tail catches up
                if( g->ENEMIES[ i ].stretch < 0 ) g->ENEMIES[ i ].stretch = 0;
            }
        }
        else
        {
            g->ENEMIES[ i ].z -= speed;
        }

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
        // claw is mid-jump (hop over the camper); P2 mirrors the test
        if( g->ENEMIES[ i ].z < 0 )
        {
            g->ENEMIES[ i ].z = 0;
            float diff = g->player_lane - g->ENEMIES[ i ].lane;
            if( diff < 0 ) diff = -diff;
            if( diff > LANES / 2 ) diff = LANES - diff;
            if( diff < 0.7 && g->jump_timer <= 0 ) hit_player( g, 0 );
            if( g->twoplayer && !g->p2_out )
            {
                float d2 = g->p2_lane - g->ENEMIES[ i ].lane;
                if( d2 < 0 ) d2 = -d2;
                if( d2 > LANES / 2 ) d2 = LANES - d2;
                if( d2 < 0.7 && g->p2_jump_timer <= 0 ) hit_player( g, 1 );
            }
        }
    }
}

// distance along the strand between neighbouring inchworm segments:
// half the body's stretch (head, middle, tail), never closer than
// WORM_MIN_GAP. Shared by the hit test, the knock-back and the renderer
// so what you see is exactly what you can hit.
float worm_gap( Enemy* e )
{
    float gap = e->stretch * 0.5;
    return gap > WORM_MIN_GAP ? gap : WORM_MIN_GAP;
}

void update_bullets( G* g )
{
    int i; int j;
    for( i = 0; i < MAX_BULLETS; i++ )
    {
        if( !g->BULLETS[ i ].alive ) continue;

        // COLLIDE FIRST, ADVANCE LAST. The old order advanced z
        // BEFORE testing, so a bullet spawned at z=0.02 was first
        // tested at z=0.055 — already past the 0.03 hit window of
        // anything camped at the rim (z=0). Enemies that reached
        // the top of the web were literally untouchable: shots
        // only ever connected with enemies still climbing. Testing
        // at the bullet's CURRENT position makes a rim camper
        // killable in its lane the moment you fire.

        // spikes eat bullets: trim the spike, kill the shot
        int li = ( (int)g->BULLETS[ i ].lane ) % LANES;
        if( g->SPIKE[ li ] > 0 && g->BULLETS[ i ].z >= 1.0 - g->SPIKE[ li ] )
        {
            // a trim that leaves only a harmless stub clears the lane:
            // spikes are rarely a multiple of 0.10 long, and a leftover
            // shorter than one bullet step (0.035) sat beyond the last
            // point a shot is ever tested -- visible forever, unhittable
            g->SPIKE[ li ] -= 0.10;
            if( g->SPIKE[ li ] < SPIKE_STUB ) g->SPIKE[ li ] = 0;
            project( g, g->BULLETS[ i ].lane, g->BULLETS[ i ].z );
            burst( g, g->px, g->py, 6, 1.5 );
            g->BULLETS[ i ].alive = 0;
            if( g->BULLETS[ i ].owner == 0 ) g->score += 5;
            else                             g->p2_score += 5;
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
                collect_powerup( g, p, g->BULLETS[ i ].owner );
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
            // inchworm: the body is a chain of segments trailing BEHIND
            // the head (z .. z + gap * (segs - 1)); a bullet anywhere
            // along it connects
            float dzwin = 0.03;
            if( g->ENEMIES[ j ].type == 4 )
            {
                Enemy* w = &g->ENEMIES[ j ];
                float back = g->BULLETS[ i ].z - w->z;      // behind the head
                float span = worm_gap( w ) * ( w->segs - 1 );
                dz = ( back >= 0 && back <= span ) ? 0 : ( back < 0 ? -back : back - span );
            }
            if( dz < dzwin && dl < 0.6 )
            {
                g->BULLETS[ i ].alive = 0;
                project( g, g->ENEMIES[ j ].lane, g->ENEMIES[ j ].z );

                // INCHWORM with segments to spare: the shot destroys
                // the FRONT segment only. The next one back becomes the
                // head exactly where it already was, so the worm is
                // knocked back a segment and keeps coming -- several
                // hits to finish, like chipping down a spike.
                if( g->ENEMIES[ j ].type == 4 && g->ENEMIES[ j ].segs > 1 )
                {
                    Enemy* w = &g->ENEMIES[ j ];
                    explode( g, g->px, g->py, 10, 2.0 );
                    sfx( g, BOOM );
                    w->z += worm_gap( w );
                    if( w->z > 1.0 ) w->z = 1.0;
                    w->stretch *= 0.5;
                    w->segs--;
                    if( g->BULLETS[ i ].owner == 0 ) g->score += 50;
                    else                             g->p2_score += 50;
                    break;
                }

                explode( g, g->px, g->py, 26, 3.0 );
                sfx( g, BOOM );

                if( g->ENEMIES[ j ].type == 1 )
                {
                    // tanker splits into two flippers
                    g->ENEMIES[ j ].type = 0;
                    if( g->BULLETS[ i ].owner == 0 ) g->score += 100;
                    else                             g->p2_score += 100;
                    spawn_near( g, g->ENEMIES[ j ].lane + 1, g->ENEMIES[ j ].z );
                }
                else
                {
                    g->ENEMIES[ j ].alive = 0;
                    if( g->BULLETS[ i ].owner == 0 ) g->score += 100;
                    else                             g->p2_score += 100;
                    if( g->spawn_timer > 999 && count_enemies( g ) == 0 )
                        level_clear( g );
                }
                break;
            }
        }

        // advance LAST: collisions were tested at the bullet's
        // current position; move it now for next frame and retire
        // it at the far end
        g->BULLETS[ i ].z += 0.035;
        if( g->BULLETS[ i ].z > 1.0 )
        {
            g->BULLETS[ i ].alive = 0;
            // a shot that makes it to the far end clears whatever
            // spike stub is left there (one too short to be tested
            // above: a spiker that died just after entering the lane)
            if( g->SPIKE[ li ] > 0 && g->SPIKE[ li ] < SPIKE_STUB ) g->SPIKE[ li ] = 0;
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
    // level bonus: paid only to players still IN the run — an out
    // player's tally FREEZES at their final score (no riding the
    // team's progress after losing the last life). Solo P1 is never
    // flagged out, so its bonus is unchanged.
    if( !g->p1_out ) g->score += 1000 + g->level * 250;
    if( g->twoplayer && !g->p2_out ) g->p2_score += 1000 + g->level * 250;
}
