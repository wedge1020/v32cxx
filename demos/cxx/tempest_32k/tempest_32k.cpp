// *****************************************************************************
//  TEMPEST 32K  —  a Tempest 2000-style tunnel shooter for Vircon32,
//  written in the v32c++ subset, rendered entirely with ASCII glyphs from
//  the BIOS font texture (-1). No custom textures, no 3D hardware.
//  Revision: walker tumble + pause text size 14 + busy-frame draw culls.
//
//  Build:  v32c++ -I ../../.. -I inc -o obj/tempest_32k.c tempest_32k.cpp
//          (see the Makefile; add -D PROFILE for the profiling build)
//
//  Fake-3D technique:
//    * The web is N lanes around a center point. A point is (lane, z in 0..1).
//      We lerp the radius between an outer (near) ellipse and an inner (far)
//      ellipse as z grows — the "flying into the tunnel" perspective without
//      any matrix math. Glyph size lerps the same way; that alone sells depth.
//    * Tunnel edges / depth rings are dotted chains of scaled '.' glyphs.
//    * All glow (particles, bullets, party text) uses ADD blending + hue
//      cycling — the whole Tempest-2000 look.
//
//  WHAT THIS DEMO SHOWS OFF (v32c++ 20261001-dev and later):
//    * The C++-side preprocessor. Tunables are #define macros that the
//      parser now SEES: they size arrays (Bullet BULLETS[ MAX_BULLETS ],
//      float SHAPE[ LANES ]), they're typed (CAM_SWAY is a float), and
//      they keep their names in the generated C. #if / #elif / #error
//      check configuration at transpile time (GLOW_MODE, LANES), and
//      `v32c++ -D PROFILE` builds the profiling variant (see main).
//    * Function-like macros as forced inlining (LERP, SIN32, COS32,
//      RGB): the Vircon32 C compiler never inlines, so the hottest
//      one-liners are spelled as macros and cost no call at all; RGB()
//      of constants folds to a single immediate.
//    * The SDK headers' own names (channel_playing, ...) -- v32c++ now
//      reads the .h headers it passes through.
//    * Ternaries anywhere, including chains -- lowered to if/else for
//      Vircon32 C, which has no `?:`.
//    * Plain float arithmetic: `t / 34.0` is float division, as written.
//
//  Vircon32 platform notes:
//    * ALL mutable game state lives in a heap-allocated `struct G`
//      (`new G`), with a `G*` threaded through every function -- one
//      allocation, one pointer, easy to save/inspect. (An older note
//      here claimed file-scope variables were read-only ROM and const
//      reads were rejected; neither is true of the current Vircon32 C
//      compiler -- globals live in RAM -- but the single-struct design
//      stands on its own.)
//    * Parameters and return values are one word: everything big goes
//      by pointer.
//
//  Other Vircon32 API notes (checked against the SDK headers):
//    * No `color` type: colors are plain ints, ABGR word order, red low byte.
//    * set_multiply_color(int); set_blending_mode(int) -- 0 = solid,
//      v32::BlendAlpha = 0x20, v32::BlendAdd = 0x21.
//    * BIOS font: texture -1 has ONE REGION PER CHARACTER -- select_region
//      with the character code, then draw_region_zoomed_at (the plain
//      DrawRegion command ignores the scale ports). Base glyph is 10x20 px.
//
//  v32c++ subset compliance:
//    * No STL, no templates.
//    * `main` is void (transpiler emits `void main(void)`).
// *****************************************************************************

// MULTI-FILE LAYOUT: v32c++ resolves and inlines .hpp/.cpp includes itself
// (the Vircon32 C compiler then sees one translation unit -- the standard
// Vircon32 way to pool sources). The headers live in inc/ (found through
// `-I inc`), each guarded by #pragma once; the modules live in src/ and are
// pooled here in dependency order, so each one sees everything it uses
// already defined.
#include "tempest.hpp"      // inc/: SDK + v32 headers, config.hpp, state.hpp

#include "src/math.cpp"          // math helpers: lerp, sine tables, hot-path inlining macros, RNG
#include "src/draw.cpp"          // colors and the draw primitives: glyphs, bars, text, the debug meters
#include "src/web.cpp"           // web geometry: the projection, the sixteen web shapes, rim runs
#include "src/audio.cpp"         // sound effects and the music track succession
#include "src/world.cpp"         // messages, enemy spawning, particle bursts, shockwaves
#include "src/player.cpp"        // firing, the superzapper, claw movement and jumping, the CPU claw, hits
#include "src/powerups.cpp"      // power-up pods and the AI buddy drone
#include "src/enemies.cpp"       // enemy behaviour, bullets and their collisions, level clear
#include "src/effects.cpp"       // shockwave, particle and starfield updates; the starfield renderer
#include "src/title.cpp"         // the title screen
#include "src/render.cpp"        // gameplay rendering: web, spikes, claws, enemies, bullets, effects
#include "src/hud.cpp"           // HUD, messages, pause menu, initials entry, high scores, level select
#include "src/game.cpp"          // level flow, spawning schedule, high-score card storage, initial state

void main()
{
    select_texture( -1 );           // BIOS font
    select_gamepad( 0 );

    // ALL mutable state lives in one heap object (see the header notes)
    G* g = new G;
    init_state( g );          // sets state 4 = title screen; heap-safe boot

    while( 1 )
    {
        // PAD CONNECTIVITY, sampled once per frame: a player whose
        // port has no gamepad is CPU-driven this frame (the watered-
        // down ai_claw). Hot-plugging works — the claw switches
        // hands the frame after the pad state changes. pad_connected
        // restores pad 0 as the selected pad on every path.
        g->p1_cpu = 0;
        if( !pad_connected( 0 ) ) g->p1_cpu = 1;
        g->p2_cpu = 0;
        if( !pad_connected( 1 ) ) g->p2_cpu = 1;

#ifdef PROFILE
        // PROFILING BUILD (v32c++ -D PROFILE ...): nobody ever runs out
        // of lives, so a scripted CPU-vs-CPU run stays in the thick of
        // play -- deaths and their explosions included -- instead of
        // ending at GAME OVER. tools/vircon32/v32prof drives it.
        if( g->lives < 3 ) g->lives = 3;
        if( g->p2_lives < 3 ) g->p2_lives = 3;
#endif

        // -- update ---------------------------------------------------------
        if( g->state == 0 )
        {
            if( gamepad_button_start() == 1 ) g->state = 5;   // pause
            else
            {
                update_player( g );
                // co-op: P2 manages its own pad selection (human
                // pad 1, or the CPU claw when port 1 is empty)
                if( g->twoplayer ) update_player2( g );
                update_spawning( g );
                update_enemies( g );
                update_bullets( g );
                update_powerups( g );
                update_buddy( g );
            }
        }
        else if( g->state == 1 )      // dying
        {
            g->state_timer--;
            if( g->state_timer <= 0 )
            {
                if( g->lives <= 0 )
                {
                    // a CPU-driven P1 (no pad on port 0) cannot post
                    // a high score — zero the tally so state 3 sends
                    // the run straight back to the title screen
                    if( g->p1_cpu ) g->score = 0;
                    g->state = 3;
                    show_message( g, "GAME OVER" );
                    g->state_timer = 300;
                    play_title_music( g );   // gameplay succession ends here
                }
                else start_level( g );
            }
        }
        else if( g->state == 2 )      // warp-out: web flies past the claw
        {
            // two phases: A — the old web streams outward off-screen;
            // B — the next level's web shrinks back in to settle.
            // At the halfway point the level number and silhouette
            // switch, so what flies IN is the new level.
            g->warp += 0.0125;
            // the claw stays live during the fly-out: you can slide
            // between lanes and JUMP — the old web's spikes stream
            // outward with it and sweep past the claw plane
            if( g->p1_cpu ) ai_claw( g, 0 );
            else
            {
                move_claw( g );
                claw_jump( g );
            }
            // co-op: P2's claw rides the transition on pad 1 too
            // (level-end webs have no enemies, so a CPU P2 just
            // holds its lane here)
            if( g->twoplayer && !g->p2_out )
            {
                if( g->p2_cpu ) ai_claw( g, 1 );
                else
                {
                    select_gamepad( 1 );
                    move_claw2( g );
                    claw_jump2( g );
                    select_gamepad( 0 );
                }
            }
            // SPIKE SWEEP (phase A): as the old web streams outward, the
            // tip of any spike sweeps past the claw plane. The crossing
            // is edge-detected per lane from fly_factor (a pure function
            // of warp), so dodging into a DIFFERENT spiked lane still
            // gets you at its own crossing frame — and a jump clears it
            // cleanly, since airborne frames are invulnerable.
            //   EASY        — deflected: replay the same level
            //   MEDIUM/HARD — killed: lose a life AND replay the level
            //   In co-op medium/hard the STRUCK player pays but the warp
            //   carries on — one player's lapse must not cancel the
            //   whole team's transition.
            if( g->warp_phase == 0 && g->warp_bounce == 0 )
            {
                int li = (int)g->player_lane;
                if( li >= LANES ) li -= LANES;
                if( g->SPIKE[ li ] > 0.05 && g->jump_timer <= 0 &&
                    ( !g->twoplayer || !g->p1_out ) )
                {
                    float ztip = 1.0 - g->SPIKE[ li ];
                    float rad = lerp( OUT_RX, IN_RX, ztip );
                    float wprev = g->warp - 0.0125;
                    if( wprev < 0 ) wprev = 0;
                    float fprev = 1.0 + wprev * wprev * 12.0;
                    if( rad * fly_factor( g ) >= OUT_RX &&
                        rad * fprev < OUT_RX )
                    {
                        if( g->difficulty == 0 )
                        {
                            g->warp_bounce = 1;
                            project( g, g->player_lane, 0 );
                            explode( g, g->px, g->py, 30, 3 );
                            shockwave( g, g->px, g->py );
                            show_message( g, "SPIKED! REPLAY LEVEL" );
                            sfx( g, BOOM );
                        }
                        else if( g->twoplayer )
                        {
                            hit_player( g, 0 );
                            show_message( g, "SPIKED!" );
                        }
                        else
                        {
                            // kill, but do NOT advance the level —
                            // start_level respawns on the same web
                            g->warp = 0;
                            g->warp_phase = 0;
                            kill_player( g );
                            show_message( g, "SPIKED!" );
                        }
                    }
                }
            }
            // P2's own sweep check: same edge detection against P2's
            // lane and jump state (the easy bounce is team-wide, so
            // warp_bounce still gates it)
            if( g->twoplayer && !g->p2_out &&
                g->warp_phase == 0 && g->warp_bounce == 0 )
            {
                int li2 = (int)g->p2_lane;
                if( li2 >= LANES ) li2 -= LANES;
                if( g->SPIKE[ li2 ] > 0.05 && g->p2_jump_timer <= 0 )
                {
                    float ztip = 1.0 - g->SPIKE[ li2 ];
                    float rad = lerp( OUT_RX, IN_RX, ztip );
                    float wprev = g->warp - 0.0125;
                    if( wprev < 0 ) wprev = 0;
                    float fprev = 1.0 + wprev * wprev * 12.0;
                    if( rad * fly_factor( g ) >= OUT_RX &&
                        rad * fprev < OUT_RX )
                    {
                        if( g->difficulty == 0 )
                        {
                            g->warp_bounce = 1;
                            project( g, g->p2_lane, 0 );
                            explode( g, g->px, g->py, 30, 3 );
                            shockwave( g, g->px, g->py );
                            show_message( g, "SPIKED! REPLAY LEVEL" );
                            sfx( g, BOOM );
                        }
                        else
                        {
                            hit_player( g, 1 );
                            show_message( g, "SPIKED!" );
                        }
                    }
                }
            }
            if( g->warp >= 1.0 && g->warp_phase == 0 )
            {
                if( g->warp_bounce == 0 )
                {
                    g->level++;
                    make_shape( g );
                }
                else g->warp_bounce = 0;   // bounced: same web flies back in
                g->warp_phase = 1;
            }
            g->state_timer--;
            if( g->state_timer <= 0 )
            {
                g->warp = 0;
                start_level( g );   // warp_phase reset inside
            }
        }
        else if( g->state == 3 )      // game over -> initials entry / title
        {
            g->state_timer--;
            if( g->state_timer <= 0 )
            {
                // did the run make the top-5 table? (score > 0 and it
                // beats an existing entry; empty slots read as 0)
                int k = -1;
                if( g->score > 0 )
                {
                    int q;
                    for( q = 0; q < 5; q++ )
                        if( g->score > g->HISCORE[ q ] ) { k = q; break; }
                }
                if( k >= 0 )
                {
                    g->hs_rank = k;
                    g->entry_pos = 0;
                    g->entry_letters[ 0 ] = 'A';
                    g->entry_letters[ 1 ] = 'A';
                    g->entry_letters[ 2 ] = 'A';
                    g->state = 6;   // prompt for initials
                }
                else
                {
                    // init_state resets difficulty to MEDIUM and
                    // players to 1 — keep the player's last choices
                    // across games
                    int dsave = g->difficulty;
                    int psave = g->players;
                    init_state( g );
                    g->difficulty = dsave;
                    g->players = psave;
                }
            }
        }
        else if( g->state == 4 )      // title screen
        {
            // title theme loops; (re)start it if it isn't running
            if( get_channel_state( 0 ) != channel_playing ) play_title_music( g );
            if( g->menu_cooldown > 0 ) g->menu_cooldown--;
            if( g->menu_cooldown == 0 )
            {
                if( gamepad_up() > 0 )
                { g->menu_row--; if( g->menu_row < 0 ) g->menu_row = 4;
                  g->menu_cooldown = 10; sfx( g, BLIP ); }
                else if( gamepad_down() > 0 )
                { g->menu_row++; if( g->menu_row > 4 ) g->menu_row = 0;
                  g->menu_cooldown = 10; sfx( g, BLIP ); }
                else if( g->menu_row == 1 && gamepad_left() > 0 )
                { g->players = 1; g->menu_cooldown = 10; sfx( g, BLIP ); }
                else if( g->menu_row == 1 && gamepad_right() > 0 )
                { g->players = 2; g->menu_cooldown = 10; sfx( g, BLIP ); }
                else if( g->menu_row == 4 && gamepad_left() > 0 )
                { g->difficulty--; if( g->difficulty < 0 ) g->difficulty = 2;
                  g->menu_cooldown = 10; sfx( g, BLIP ); }
                else if( g->menu_row == 4 && gamepad_right() > 0 )
                { g->difficulty++; if( g->difficulty > 2 ) g->difficulty = 0;
                  g->menu_cooldown = 10; sfx( g, BLIP ); }
            }
            if( gamepad_button_a() == 1 || gamepad_button_start() == 1 )
            {
                if( g->menu_row == 0 )
                {
                    // PLAY: 2-player setup arms the co-op web — a
                    // missing pad on either port hands that claw to
                    // the CPU (see update_player/update_player2)
                    if( g->players == 2 ) start_coop( g );
                    play_track( g, level_track( g ) );   // begin the track run
                    start_level( g );
                }
                else if( g->menu_row == 2 ) g->state = 7;   // high scores
                else if( g->menu_row == 3 )
                {
                    g->select_level = 1;
                    g->level = 1;
                    make_shape( g );      // preview data for the screen
                    g->state = 8;         // level select
                    sfx( g, BLIP );
                }
                // rows 1 (players) and 4 (difficulty): A does
                // nothing — LEFT/RIGHT changes them
            }
        }
        else if( g->state == 6 )      // new high score: initials entry
        {
            if( get_channel_state( 0 ) != channel_playing ) play_title_music( g );
            if( g->menu_cooldown > 0 ) g->menu_cooldown--;
            if( g->menu_cooldown == 0 )
            {
                if( gamepad_up() > 0 )
                {
                    int l = g->entry_letters[ g->entry_pos ] + 1;
                    // 'A'..'Z' then '0'..'9'. All letters are ABOVE '9'
                    // in ASCII, so a bare "l > '9'" catches every letter
                    // and pins the cursor at 'A' — the gap test must be
                    // the RANGE between '9' and 'A' (chars ':' .. '@').
                    if( l > 'Z' ) l = '0';
                    else if( l > '9' && l < 'A' ) l = 'A';
                    g->entry_letters[ g->entry_pos ] = l;
                    g->menu_cooldown = 9; sfx( g, BLIP );
                }
                else if( gamepad_down() > 0 )
                {
                    int l = g->entry_letters[ g->entry_pos ] - 1;
                    // mirror of the UP wrap: below '0' jumps to 'Z',
                    // and the 'A' -> '9' step uses the same gap RANGE
                    // (digits are all BELOW 'A', so a bare "l < 'A'"
                    // would catch every digit and pin at '9').
                    if( l < '0' ) l = 'Z';
                    else if( l > '9' && l < 'A' ) l = '9';
                    g->entry_letters[ g->entry_pos ] = l;
                    g->menu_cooldown = 9; sfx( g, BLIP );
                }
                else if( gamepad_left() > 0 )
                { g->entry_pos--; if( g->entry_pos < 0 ) g->entry_pos = 2;
                  g->menu_cooldown = 9; sfx( g, BLIP ); }
                else if( gamepad_right() > 0 )
                { g->entry_pos++; if( g->entry_pos > 2 ) g->entry_pos = 0;
                  g->menu_cooldown = 9; sfx( g, BLIP ); }
            }
            if( gamepad_button_a() == 1 || gamepad_button_start() == 1 )
            {
                // insert the score at its rank, shifting the rest down
                int k = g->hs_rank;
                int i;
                for( i = 4; i > k; i-- )
                {
                    g->HISCORE[ i ] = g->HISCORE[ i - 1 ];
                    g->HIINIT[ i ][ 0 ] = g->HIINIT[ i - 1 ][ 0 ];
                    g->HIINIT[ i ][ 1 ] = g->HIINIT[ i - 1 ][ 1 ];
                    g->HIINIT[ i ][ 2 ] = g->HIINIT[ i - 1 ][ 2 ];
                }
                g->HISCORE[ k ] = g->score;
                g->HIINIT[ k ][ 0 ] = g->entry_letters[ 0 ];
                g->HIINIT[ k ][ 1 ] = g->entry_letters[ 1 ];
                g->HIINIT[ k ][ 2 ] = g->entry_letters[ 2 ];
                hs_save( g );
                g->state = 7;   // show the updated table
                sfx( g, CLEAR );
            }
        }
        else if( g->state == 7 )      // high scores table
        {
            if( get_channel_state( 0 ) != channel_playing ) play_title_music( g );
            if( gamepad_button_a() == 1 || gamepad_button_b() == 1 ||
                gamepad_button_start() == 1 )
            {
                int dsave = g->difficulty;
                int psave = g->players;
                init_state( g );
                g->difficulty = dsave;
                g->players = psave;
            }
        }
        else if( g->state == 8 )      // level select
        {
            if( get_channel_state( 0 ) != channel_playing ) play_title_music( g );
            if( g->menu_cooldown > 0 ) g->menu_cooldown--;
            if( g->menu_cooldown == 0 )
            {
                if( gamepad_right() > 0 )
                {
                    g->select_level++;
                    if( g->select_level > 32 ) g->select_level = 1;
                    g->level = g->select_level;
                    make_shape( g );      // refresh the rotating preview
                    g->menu_cooldown = 10; sfx( g, BLIP );
                }
                else if( gamepad_left() > 0 )
                {
                    g->select_level--;
                    if( g->select_level < 1 ) g->select_level = 32;
                    g->level = g->select_level;
                    make_shape( g );
                    g->menu_cooldown = 10; sfx( g, BLIP );
                }
            }
            if( gamepad_button_a() == 1 || gamepad_button_start() == 1 )
            {
                g->level = g->select_level;
                // honor the PLAYERS selector here too — testing a
                // specific level with a CPU partner works the same
                // as the PLAY path
                if( g->players == 2 ) start_coop( g );
                play_track( g, level_track( g ) );   // begin the track run
                start_level( g );
            }
            else if( gamepad_button_b() == 1 )
            {
                g->level = 1;          // restore the normal PLAY path
                make_shape( g );
                g->state = 4;
            }
        }
        else if( g->state == 5 )      // PAUSE: music keeps playing
        {
            if( g->menu_cooldown > 0 ) g->menu_cooldown--;
            if( g->menu_cooldown == 0 )
            {
                // LEFT/RIGHT: music volume. L/R buttons: SFX volume.
                // UP/DOWN: soundtrack (switches live).
                if( gamepad_right() > 0 )
                {
                    g->music_volume += 0.1;
                    if( g->music_volume > 2.0 ) g->music_volume = 2.0;
                    select_channel( 0 );
                    set_channel_volume( g->music_volume );
                    g->menu_cooldown = 8; sfx( g, BLIP );
                }
                else if( gamepad_left() > 0 )
                {
                    g->music_volume -= 0.1;
                    if( g->music_volume < 0 ) g->music_volume = 0;
                    select_channel( 0 );
                    set_channel_volume( g->music_volume );
                    g->menu_cooldown = 8; sfx( g, BLIP );
                }
                else if( gamepad_button_r() > 0 )
                {
                    g->sfx_volume += 0.1;
                    if( g->sfx_volume > 2.0 ) g->sfx_volume = 2.0;
                    g->menu_cooldown = 8;   // BLIP plays at the new level
                    sfx( g, BLIP );
                }
                else if( gamepad_button_l() > 0 )
                {
                    g->sfx_volume -= 0.1;
                    if( g->sfx_volume < 0 ) g->sfx_volume = 0;
                    g->menu_cooldown = 8;
                    sfx( g, BLIP );
                }
                else if( gamepad_down() > 0 )
                {
                    int t = g->music_index + 1;
                    if( t > 3 ) t = 0;
                    play_track( g, t );
                    // manual choice: LOCK the track against the band
                    // stamping until the next band edge
                    g->track_lock = 1;
                    g->track_lock_band = level_track( g );
                    g->menu_cooldown = 12; sfx( g, BLIP );
                }
                else if( gamepad_up() > 0 )
                {
                    int t = g->music_index - 1;
                    if( t < 0 ) t = 3;
                    play_track( g, t );
                    g->track_lock = 1;
                    g->track_lock_band = level_track( g );
                    g->menu_cooldown = 12; sfx( g, BLIP );
                }
            }
            if( gamepad_button_start() == 1 ) g->state = 0;
        }
        if( g->state != 5 )   // paused: the whole world freezes
        {
            update_particles( g );
            update_shocks( g );
            update_stars( g );
        }
        if( g->state != 3 && g->state != 4 && g->state != 6 &&
            g->state != 7 && g->state != 8 )
            update_music( g );   // track succession (incl. during pause;
                                // menus keep the looping title theme)
        g->frame++;

        // -- render ---------------------------------------------------------
        // DEBUG METERS: the CPU cost model accumulator is reset before
        // the first draw (instrumented draws add their model cost as
        // they happen), and the GPU pixel counter is sampled BEFORE
        // the clear — that opening reading is this frame's pixel
        // budget, so the GPU line self-calibrates (see gpu_meter)
        g->cpu_cycles = 0;
        // insurance: force known GPU state before the clear, so no
        // stale blending mode or multiply color can interfere with it
        set_blending_mode( BLEND_SOLID );
        set_multiply_color( RGB( 255, 255, 255 ) );
        g->gpixels_start = gpu_remaining_pixels();
        clear_screen( RGB( 2, 2, 8 ) );
        g->cpu_cycles += 4000;   // CPU meter: flat charge, hw clear path
        // (g->partcount -- live particles, read by several render-phase
        // throttles -- is kept current by update_particles; a paused
        // world doesn't change it)
        render_starfield( g );
        if( g->state == 4 )
        {
            render_title( g );
            end_frame();
            continue;
        }
        if( g->state == 6 )
        {
            render_entry( g );
            end_frame();
            continue;
        }
        if( g->state == 7 )
        {
            render_scores( g );
            end_frame();
            continue;
        }
        if( g->state == 8 )
        {
            render_levelselect( g );
            end_frame();
            continue;
        }
        render_web( g );
        render_spikes( g );
        render_bullets( g );
        render_enemies( g );
        render_powerups( g );
        render_buddy( g );
        render_player( g );
        render_particles( g );
        render_shocks( g );
        render_hud( g );
        if( g->state == 5 ) render_pause( g );
        render_message( g );
        if( g->debug_gpu ) gpu_meter( g );

        end_frame();
    }
}
