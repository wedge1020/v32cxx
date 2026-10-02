// *****************************************************************************
//  TEMPEST 32K -- src/game.cpp
//  level flow, spawning schedule, high-score card storage, initial state
// *****************************************************************************
#include "tempest.hpp"

// ---------------------------------------------------------------------------
//  Level flow
// ---------------------------------------------------------------------------
// begin a 2-player co-op run: P2's claw starts on the opposite side
// of the web. Lives and scores are tracked per player; the pods, the
// superzaps and the level bonus are shared by the team.
void start_coop( G* g )
{
    g->twoplayer = 1;
    g->p2_lives = 3;
    g->p2_score = 0;
    g->p2_lane = g->player_lane + 8;
    if( g->p2_lane >= LANES ) g->p2_lane -= LANES;
    g->p2_jump_timer = 0;
    g->p2_jump_cooldown = 0;
    g->p2_fire_cooldown = 0;
    g->p2_respawn = 0;
    g->p2_out = 0;
    g->p1_respawn = 0;
    g->p1_out = 0;
}

void start_level( G* g )
{
    int i;
    for( i = 0; i < MAX_ENEMIES; i++ ) g->ENEMIES[ i ].alive = 0;
    for( i = 0; i < MAX_BULLETS; i++ ) g->BULLETS[ i ].alive = 0;
    int zaps = 2;
    if( g->difficulty == 0 ) zaps = 3;
    if( g->difficulty == 2 ) zaps = 1;
    g->superzaps = zaps;
    for( i = 0; i < LANES; i++ ) g->SPIKE[ i ] = 0;
    g->launched = 0;
    make_shape( g );
    // MUSIC BAND STAMPING — but a track the player picked in the
    // pause menu OVERRIDES the band: the choice survives level
    // starts until the run crosses a real band edge (the hard
    // shifts at 9/17/25/1), and only there does the band track
    // take over again.
    int bt = level_track( g );
    if( g->track_lock == 1 )
    {
        if( bt != g->track_lock_band )
        {
            g->track_lock = 0;
            play_track( g, bt );
        }
    }
    else if( bt != g->music_index ) play_track( g, bt );
    g->warp = 0;              // insurance: no transition residue in play
    g->warp_phase = 0;
    g->warp_bounce = 0;
    g->spawn_interval = 70 - g->level * 2;
    // difficulty pacing: easy breathes, hard floods
    if( g->difficulty == 0 ) g->spawn_interval += 24;
    if( g->difficulty == 2 ) g->spawn_interval -= 12;
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
        // CONCURRENCY CAP: however fast the spawn clock runs, only
        // so many enemies may be ALIVE at once. This is the real
        // "easy stays playable at level 32" guarantee — on the high
        // levels the spawn rate alone let swarms stack up far faster
        // than they could be shot, and the field flipped from calm
        // to hopeless in a few seconds. A blocked spawn retries
        // shortly and does NOT consume the level's quota. Tanker
        // splits bypass this (spawn_near) — a split is one launched
        // enemy becoming two.
        int cap = 10;
        if( g->difficulty == 0 ) cap = 5;
        if( g->difficulty == 2 ) cap = 14;
        if( count_enemies( g ) >= cap )
        {
            g->spawn_timer = 12;
            return;
        }
        g->spawn_timer = g->spawn_interval;
        int roll = rng( g ) % 7;
        int tanker = ( roll == 0 );
        int spiker = ( roll == 1 );
        // WALKERS ('X'): a rim patrol that tumbles end-over-end —
        // max two at once; they are persistent lane hazards
        int walker = ( roll == 2 && count_walkers( g ) < 2 );
        // INCHWORMS ('W' electric): the stretch-surge climber —
        // from level 5, max two at once
        int worm = ( roll == 3 && g->level >= 5 && count_worms( g ) < 2 );
        // first match wins: spiker, walker, worm, tanker, else flipper
        int etype = spiker ? 2 : walker ? 3 : worm ? 4 : tanker ? 1 : 0;
        spawn_enemy( g, etype );
        int quota = 18 + g->level;
        if( g->difficulty == 0 ) quota = 14 + g->level;
        if( g->difficulty == 2 ) quota = 23 + g->level;
        g->launched++;
        if( g->launched >= quota )
        {
            g->spawn_timer = 999999;   // stop spawning; level ends when clear
            g->launched = 0;
        }
    }
}

// ---------------------------------------------------------------------------
//  High scores — persisted on the Vircon32 memory card (raw layout; the
//  card is a flat 32K-BYTE space = 8192 WORDS, and ALL memcard offsets
//  and sizes are in WORDS — see the SDK's card_read_data asm: "movs"
//  copies CR words; card_read_signature uses CR=20 for int[20]. Mixing
//  in byte counts overflows the destination and SMASHES THE STACK —
//  a 20-word buffer fed size=80 blew 60 words of stack, corrupting
//  the return address: instant silent CPU halt at boot):
//    words  0..19 : game signature, 20 words spelling "TEMPEST32KHS"
//    words 20..24 : HISCORE[5]
//    words 25..39 : HIINIT[5][3] as character codes
//  A card whose signature doesn't match is treated as empty (defaults),
//  and the first save claims the area. No card = defaults, saves skipped.
//  Only card_is_connected/card_read_data/card_write_data are used, all
//  with int* arguments — card_signature_matches' typedef'd pointer type
//  (int[20]*) is deliberately avoided.
// ---------------------------------------------------------------------------
void hs_make_sig( char* tag, int* sig )
{
    int i;
    for( i = 0; i < 20; i++ ) sig[ i ] = 0;
    i = 0;
    while( tag[ i ] != 0 && i < 20 ) { sig[ i ] = tag[ i ]; i++; }
}

void hs_load( G* g )
{
    int i; int j;
    for( i = 0; i < 5; i++ )
    {
        g->HISCORE[ i ] = 0;
        for( j = 0; j < 3; j++ ) g->HIINIT[ i ][ j ] = '-';
    }
    if( !card_is_connected() ) return;
    int tag[ 20 ];
    int got[ 20 ];
    hs_make_sig( "TEMPEST32KHS", &tag[ 0 ] );
    card_read_data( &got[ 0 ], 0, 20 );          // 20 WORDS, not bytes!
    for( i = 0; i < 20; i++ ) if( got[ i ] != tag[ i ] ) return;
    card_read_data( &g->HISCORE[ 0 ], 20, 5 );       // words 20..24
    card_read_data( &g->HIINIT[ 0 ][ 0 ], 25, 15 );  // words 25..39
}

void hs_save( G* g )
{
    if( !card_is_connected() ) return;
    int tag[ 20 ];
    hs_make_sig( "TEMPEST32KHS", &tag[ 0 ] );
    card_write_data( &tag[ 0 ], 0, 20 );
    card_write_data( &g->HISCORE[ 0 ], 20, 5 );
    card_write_data( &g->HIINIT[ 0 ][ 0 ], 25, 15 );
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
    g->proj_warp = -1.0;          // force project()'s fly cache to fill
    g->player_lane = 0;
    g->cam_x = 0;
    g->cam_y = 0;
    g->lives = 3;
    g->score = 0;
    g->level = 1;
    g->warp = 0;
    g->warp_phase = 0;
    g->warp_bounce = 0;
    g->frame = 0;
    g->rng_state = 12345;
    g->difficulty = 1;
    g->menu_cooldown = 0;
    g->state = 4;          // boot straight to the title screen
    g->message_timer = 0;
    g->message[ 0 ] = 0;
    g->last_region = -1;
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;

    // effects + power-up state: heap memory from `new` is NOT zeroed,
    // so every slot must be explicitly cleared here
    g->buddy_timer = 0;
    g->buddy_cooldown = 0;
    g->buddy_lane = 0;
    g->laser_timer = 0;
    g->rapid_timer = 0;
    g->powerup_timer = 500;
    g->jump_timer = 0;
    g->jump_cooldown = 0;
    // two-player state: cleared at boot and on every return to the
    // title flow — start_coop re-arms it when a co-op run begins
    g->twoplayer = 0;
    g->p2_lane = 8;
    g->p2_jump_timer = 0;
    g->p2_jump_cooldown = 0;
    g->p2_fire_cooldown = 0;
    g->p2_lives = 3;
    g->p2_score = 0;
    g->p1_respawn = 0;
    g->p2_respawn = 0;
    g->p1_out = 0;
    g->p2_out = 0;
    g->players = 1;         // menu default; preserved across games
    g->p1_cpu = 0;          // sampled from pad connectivity each frame
    g->p2_cpu = 0;
    // DEBUG METERS: on by default — set 0 to hide the readout
    g->debug_gpu = 1;
    g->cpu_cycles = 0;
    g->cpu_even = 0;
    g->cpu_odd = 0;
    g->cpu_parity = 0;
    g->gpixels_start = 2073600;   // placeholder; real reading on frame 1
    g->gpu_pix_even = 0;
    g->gpu_pix_odd = 0;
    g->web_ang_ok = 0;            // angle caches refill on first render_web
    g->hud_ang_ok = 0;            // HUD icon angles refill on first render_hud
    // fresh run: no manual track choice pending
    g->track_lock = 0;
    g->track_lock_band = 0;
    g->music_index = 0;
    g->sfx_channel = 2;
    g->music_volume = 0.8;
    g->sfx_volume = 0.8;
    g->menu_row = 0;
    g->select_level = 1;
    g->hs_rank = -1;
    g->entry_pos = 0;
    g->entry_letters[ 0 ] = 'A';
    g->entry_letters[ 1 ] = 'A';
    g->entry_letters[ 2 ] = 'A';
    int i;
    for( i = 0; i < LANES; i++ ) g->CONN[ i ] = 1;   // closed until make_shape
    for( i = 0; i < MAX_SHOCKS; i++ ) g->SHOCKS[ i ].alive = 0;
    for( i = 0; i < MAX_POWERUPS; i++ ) g->POWERUPS[ i ].alive = 0;
    build_tables( g );
    init_starfield( g );
    hs_load( g );
}
