// *****************************************************************************
//  TEMPEST 32K -- inc/config.hpp
//  cart metadata and resources, tunables, transpile-time configuration checks
// *****************************************************************************
#pragma once

// SPU channel states: audio.h's own channel_stopped / channel_paused /
// channel_playing (0x40..0x42) -- visible here now that v32c++ reads the
// SDK headers it passes through, so no private copies are needed.

// 0 = solid (no blending); the v32 enum only names alpha/add/subtract
#define BLEND_SOLID 0

// ---------------------------------------------------------------------------
//  Cart metadata (parsed by the v32c++ lexer; delete if your build rejects)
// ---------------------------------------------------------------------------
#title "TEMPEST 32K"
#version 0.3

// CART SOUND RESOURCES — v32c++ lexer hints (#sound NAME "file"). The
// transpiler emits `#define NAME id` in declaration order, so NAME is a
// compile-time constant usable anywhere below. IDs M_TRACK1..M_TRACK4
// are CONSECUTIVE (music succession relies on M_TRACK1 + n arithmetic).
// Generate the .wav files with the companion gen_sounds.c program.
#sound PEWPEW  "sounds/shootsound.wav"
#sound BOOM    "sounds/boom.wav"
#sound DEATH   "sounds/death.wav"
#sound ZAPSND  "sounds/zap.wav"
#sound JUMP    "sounds/jump.wav"
#sound PICKUP  "sounds/pickup.wav"
#sound CLEAR   "sounds/clear.wav"
#sound BLIP    "sounds/blip.wav"
#sound M_TITLE  "sounds/m_title.wav"
#sound M_TRACK1 "sounds/m_track1.wav"
#sound M_TRACK2 "sounds/m_track2.wav"
#sound M_TRACK3 "sounds/m_track3.wav"
#sound M_TRACK4 "sounds/m_track4.wav"

// ---------------------------------------------------------------------------
//  Tunables -- #define macros, expanded by v32c++ itself, so they work as
//  array sizes and in #if, and keep their names in the generated C
// ---------------------------------------------------------------------------
#define LANES 16          // web lanes (power of two: see project())
#define RINGS 7
#define MAX_BULLETS 48
#define MAX_ENEMIES 24
#define MAX_PARTICLES 220
#define MAX_STARS 80
#define MAX_SHOCKS 3
#define MAX_POWERUPS 4

// INCHWORM: body segments (one per hit), and the closest two neighbours
// ever sit along the strand -- so a bunched worm still reads as a chain
#define WORM_SEGMENTS 3
#define WORM_MIN_GAP 0.022
// a spike shorter than this is harmless (the warp sweep ignores it)
#define SPIKE_STUB 0.05

#define CX 320
#define CY 168
#define OUT_RX 300
#define OUT_RY 158
#define IN_RX 34
#define IN_RY 18

// CAMERA SWAY (Tempest 2000 flavour): the tube's vanishing point leans
// toward the player's lane, eased over ~20 frames. 0 disables. The
// rim (z=0) stays FIXED — only mid-tunnel and far geometry shifts, so
// gameplay readability and the hard-won bar alignment are untouched.
// 0.18 * the claw's offset-from-axis = up to ~54px of lean.
#define CAM_SWAY 0.18
#define CAM_EASE 0.05

// A/B KILL-SWITCHES for the additive-glow white-out bug, per layer.
// BISECTION RESULT: BlendAdd (0x21) white-screens the real emulator
// in this game no matter which layer uses it; BlendAlpha (0x20) works
// perfectly. A single static unscaled glyph does NOT reproduce, so
// the Add bug needs varying scale / color / position (Zoomed path?).
// DEFAULT: alpha glow everywhere.
#define GLOW_MODE  2   // 0 = solid, 1 = BlendAdd (0x21), 2 = BlendAlpha (0x20)
#define GLOW_ALL   1
#define GLOW_BULLET 0   // bullets
#define GLOW_PART   0   // particles
#define GLOW_SPIKE  0   // spikes
#define GLOW_CLAW   0   // player claw accents
#define GLOW_ENEMY  0   // enemy cores

// the blend mode enabled glow layers use, settled at transpile time
#if GLOW_MODE == 1
#define GLOW_BLEND v32::BlendAdd
#elif GLOW_MODE == 2
#define GLOW_BLEND v32::BlendAlpha
#elif GLOW_MODE == 0
#define GLOW_BLEND BLEND_SOLID
#else
#error "GLOW_MODE must be 0 (solid), 1 (add) or 2 (alpha)"
#endif
