// *****************************************************************************
//  TEMPEST 32K -- inc/state.hpp
//  entity structs and G, the one heap object holding all game state
// *****************************************************************************
#pragma once

#include "config.hpp"     // array sizes (LANES, MAX_BULLETS, ...)

// ---------------------------------------------------------------------------
//  ALL mutable game state lives here, on the heap (see header comment)
// ---------------------------------------------------------------------------
struct Bullet
{
    int alive;
    float lane;
    float z;
    int owner;              // 0 = P1, 1 = P2 (score attribution + tint)
};

struct Enemy
{
    int alive;
    int type;          // 0 = flipper, 1 = tanker, 2 = spiker, 3 = rim walker ('X'),
                       // 4 = inchworm ('W' electric) — spans the strand, surges in pulses
    float lane;
    float z;           // inchworm: the HEAD (front end)
    float stretch;     // inchworm only: body length in z units behind
                       // the head (0 bunched .. reach fully extended)
    int segs;          // inchworm only: segments left (WORM_SEGMENTS
                       // at spawn); each hit destroys the front one
    int cooldown;
    int wig;           // walkers: tumble angle. inchworms: phase clock
                       // (0..63; 0..31 = head surge, 32..63 = tail catch-up)
    int dir;           // patrol direction (+1/-1): spikers mid-tunnel,
                       // rim walkers along the edge; both reverse at gaps
};

struct Particle
{
    int alive;
    float x; float y;
    float vx; float vy;
    int life;
    int glyph;
};

struct Shock
{
    int alive;
    float x; float y;
    int life;
};

// background starfield: screen-space stars streaming outward from the
// vanishing point (matches the fly-into-the-tunnel camera), streaking
// during warp-out. Bright and plainly visible — the original dim/slow
// version was practically invisible against the dark background.
struct Star
{
    float x; float y;
    float spd;               // radial px/frame
    float dirx; float diry;  // cached UNIT direction from the web's
                             // vanishing point (see update_stars)
};

struct PowerUp
{
    int alive;
    int type;        // 0 = superzap, 1 = AI buddy, 2 = extra life,
                     // 3 = super laser, 4 = rapid blaster
    float lane;
    float z;
};

struct G
{
    // math tables
    float SIN_TABLE[ 256 ];
    float COS_TABLE[ 256 ];

    // web geometry
    float SHAPE[ LANES ];
    float SHAPE_Y[ LANES ];   // vertical radius factor: 1.0 on the CLASSIC
                           // pass (levels 1-16) so the shape modulates
                           // only x; equal to SHAPE on the TRUE pass
                           // (17-32) so webs trace their real polygon
    int   CONN[ LANES ];          // CONN[i]: 1 = rim edge between lanes i,i+1 exists
    float px; float py; float pscale;   // project() outputs

    // entities
    char  GLYPHS[ 6 ];
    Bullet   BULLETS[ MAX_BULLETS ];
    Enemy    ENEMIES[ MAX_ENEMIES ];
    Particle PARTICLES[ MAX_PARTICLES ];
    Shock    SHOCKS[ MAX_SHOCKS ];
    PowerUp  POWERUPS[ MAX_POWERUPS ];
    Star     STARFIELD[ MAX_STARS ];
    float SPIKE[ LANES ];

    // game state
    float player_lane;
    float cam_x; float cam_y;   // camera sway: vanishing-point offset
    int   partcount;            // particles alive — cached once per frame
    int   lives;
    int   score;
    int   level;
    int   superzaps;
    int   spawn_timer;
    int   spawn_interval;
    int   state;               // 0 play, 1 dying, 2 warp-out, 3 game over, 4 title, 5 pause, 6 initials, 7 scores, 8 level select
    int   warp_phase;          // transition: 0 = old web flying out, 1 = new web flying in
    int   warp_bounce;         // transition: spike hit on EASY — skip the level advance
    int   menu_row;            // title menu: selected row (0 play 1 scores 2 levels 3 difficulty)
    int   select_level;        // level select screen: chosen level (1..32)
    int   HISCORE[ 5 ];        // high score table, highest first (memcard)
    int   HIINIT[ 5 ][ 3 ];    // 3-letter initials per high score entry
    int   hs_rank;             // rank of the score currently being entered
    int   entry_pos;           // initials entry: cursor 0..2
    int   entry_letters[ 3 ];  // initials entry: current letters (char codes)
    int   difficulty;          // 0 easy, 1 medium, 2 hard (set on title screen)
    int   menu_cooldown;       // title screen: frames between L/R difficulty nudges
    int   state_timer;
    char  message[ 24 ];
    int   message_timer;
    float warp;
    int   launched;
    int   fire_cooldown;
    int   jump_timer;       // claw leap: frames airborne (34 total)
    int   jump_cooldown;    // frames until the next leap is allowed

    // TWO-PLAYER CO-OP: player 2's own claw, driven by gamepad 1.
    // The web, enemies and level are shared; lives and scores are
    // kept separately (P2's score rides under P1's on the HUD).
    int   twoplayer;        // 1 = co-op run in progress
    float p2_lane;
    int   p2_jump_timer;
    int   p2_jump_cooldown;
    int   p2_fire_cooldown;
    int   p2_lives;
    int   p2_score;
    int   p1_respawn;       // invulnerable blink frames after a hit
    int   p2_respawn;
    int   p1_out;           // 1 = that player is out of lives
    int   p2_out;
    int   players;          // menu selection: 1 or 2 players
    int   p1_cpu;          // 1 = no gamepad on that player's port —
    int   p2_cpu;          // the CPU drives the claw (watered-down
                           // AI: slower to move AND fire than the
                           // AI buddy drone, scaled by difficulty)
    // WEB BAR ANGLE CACHE (see render_web): the rim and far-cap bar
    // directions are per-LEVEL constants — make_shape invalidates,
    // and the first render_web of the level refills (one-time
    // segment_angle pass, instead of 32 dir_angle scans per frame)
    float RIM_ANG[ LANES ];   // cached rim bar angle, per lane
    float CAP_ANG[ LANES ];   // cached far-cap bar angle, per lane
    // WEB POINT CACHE (see render_web): this frame's projected rim
    // (z=0) and far-cap (z=1) vertex per lane -- 32 project() calls
    // per frame instead of ~96 for the spokes, bars and vertex caps
    float RIM_X[ LANES ]; float RIM_Y[ LANES ];
    float CAP_X[ LANES ]; float CAP_Y[ LANES ];
    int   web_ang_ok;      // 1 = angle caches valid for this shape
    // HUD LIVES-ICON ANGLE CACHE (see render_hud): every spare-claw
    // icon sits at a FIXED screen slot (560/100 + slot*22, y=346), so
    // each slot's aim at the web centre is a constant — filled once
    // on the first HUD draw instead of 8 forty-iteration dir_angle
    // scans EVERY frame
    float HUD1_ANG[ 8 ];    // P1 spare-claw icon angles, per slot
    float HUD2_ANG[ 8 ];    // P2 spare-claw icon angles, per slot
    int   hud_ang_ok;       // 1 = HUD angle caches filled
    // DEBUG METERS (see gpu_meter): TWO readout lines. The CPU line
    // is a heuristic cost model — 24 cycles per command plus 1 per
    // filled pixel (rotozoomed fills cost double, the slow path) —
    // compared against the 250,000-CYCLE CPU frame budget (the CPU's
    // per-frame cycle allowance at 60 fps). The GPU line is REAL
    // hardware usage: the GPU_RemainingPixels port, sampled at frame
    // start (this frame's pixel budget) and again at meter time
    // (used = start - remaining). debug_gpu = 1 draws the readout
    // top-left; flip to 0 to hide.
    int   debug_gpu;
    int   cpu_cycles;      // CPU model: this frame's running total
    int   cpu_even;        // CPU model: latest even-frame total
    int   cpu_odd;         // CPU model: latest odd-frame total
    int   cpu_parity;      // which parity the readout shows (0=E/1=O)
    int   gpixels_start;   // GPU: pixel budget at frame start (real)
    int   gpu_pix_even;    // GPU: latest even-frame pixels used
    int   gpu_pix_odd;     // GPU: latest odd-frame pixels used
    // music: a track picked in the pause menu OVERRIDES the level
    // band until the run crosses a real band edge (9/17/25/1) —
    // start_level honors the lock and clears it there
    int   track_lock;      // 1 = the player's manual choice is active
    int   track_lock_band; // band index (level_track) at lock time
    int   buddy_timer;      // AI buddy drone: frames remaining
    int   buddy_cooldown;   // AI buddy: frames until next auto-shot
    float buddy_lane;       // AI buddy: its OWN lane (eases to targets)
    int   laser_timer;      // super laser power-up: frames remaining
    int   rapid_timer;      // rapid blaster power-up: frames remaining
                             // (easy uses 999999 = effectively unlimited)
    int   powerup_timer;    // frames until the next power-up spawns
    int   music_index;      // gameplay track currently queued (0..3)
    int   sfx_channel;      // round-robin SFX channel allocator (2..13)
    float music_volume;     // channel 0 volume 0..2 (gameplay + title tracks)
    float sfx_volume;       // channels 2..13 volume 0..2 (effects)
    int   frame;
    int   rng_state;

    // project() fly-factor cache (see project_refresh): the inputs it
    // was computed from, and the result
    float proj_warp;
    int   proj_jt;
    int   proj_jt2;
    float proj_fly;

    // draw-call throttles: the last region/scale we set, so draw_glyph
    // can skip redundant GPU register writes (a big deal at hundreds of
    // glyphs per frame)
    int   last_region;
    float last_scale_x;
    float last_scale_y;
};
