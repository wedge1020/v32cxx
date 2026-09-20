#include "video.h"
#include "audio.h"
#include "input.h"
#include "misc.h"
#include "time.h"

// ============================================================================
//  SPACE INVADERS - portable object-oriented C++ skeleton
//
//  Freestanding: NO standard library headers are used. Everything needed
//  beyond core C++ (random numbers, containers) is implemented here.
//  Written to the v32c++ subset: no templates, no static members, no
//  'explicit', no default parameter values, no in-class default member
//  initializers, no class-nested enums, no 'unsigned', no ternaries, and
//  NO bare constructor-call / functional-cast expressions ("Vec2(x, y)").
//  Only `new T(args)` may construct with arguments in an expression;
//  everywhere else uses local declarations or int arguments.
//
//  Vec2 is deliberately a ONE-WORD type (a single int packing x/y as two
//  16-bit halves): the Vircon32 C compiler only accepts parameters and
//  return values of exactly one word, so a 2-int Vec2 returned by value
//  (operator+ etc.) would be rejected downstream. Packing keeps natural
//  by-value math while staying one word. Coordinates stay well inside
//  the 16-bit range (Vircon32 screen / playfield is 640 x 360).
//
//  Platform hookup notes:
//    * Video is WIRED to the Vircon32 SDK: blit() does
//      select_region(id) + draw_region_at(x, y); clear() calls
//      clear_screen(); sync() calls end_frame(). init() selects
//      texture -1 once (no texture: sprites are region-only).
//    * Input is WIRED: read() forwards directly to the per-button SDK
//      query functions (gamepad_up(), gamepad_button_a(), ...) -- each
//      already returns the signed +/- frame count the contract needs.
//    * Audio is still stubbed: sound effects are triggered via Sound::play().
//
//  NOTE ON SDK CALLS: select_region/draw_region_at/end_frame/
//  gamepad_button_state are NOT declared anywhere in this C++ source --
//  v32c++'s sema leaves unresolved free-function calls untouched and
//  codegen emits them verbatim, so they pass straight through to the
//  generated C, where the #include lines above (re-emitted verbatim at
//  the top of the output by the preprocessor pass-through) resolve them
//  against the real Vircon32 SDK headers. This is the same mechanism
//  tests/sample22.cpp and friends use for video.h/audio.h.
//
//  INPUT CONTRACT
//  --------------
//  Each button is queried individually, e.g. input.read(si::BTN_A).
//  The returned value is NEVER zero:
//      positive  N  -> button currently pressed, held for N consecutive frames
//      negative -N  -> button not pressed, released for N consecutive frames
//  Buttons: UP, DOWN, LEFT, RIGHT, START, A, B, X, Y, L, R
//
//  SPRITES: all 10 x 20 pixels, one sprite per frame.
//  Sprite ids are simply the ASCII value of the character shown.
//  (On the Vircon32 side, the cart texture's regions must be DEFINED in
//  this same id order -- region id == ASCII code -- via define_region()
//  or the Region Editor tool, so select_region(id) picks the right one.)
//  ---------------------------------------------------------------
//  char  name                 10 x 20     notes
//  ----  -------------------  --------    --------------------------------
//   '='  PLAYER_SHIP             "       player cannon
//   '!'  PLAYER_BULLET           "       player shot
//   'W'  ALIEN_A_FRAME0          "       top-row alien (squid), frame 0
//   'w'  ALIEN_A_FRAME1          "       top-row alien, frame 1
//   'X'  ALIEN_B_FRAME0          "       middle-row alien (crab), frame 0
//   'x'  ALIEN_B_FRAME1          "       middle-row alien, frame 1
//   'O'  ALIEN_C_FRAME0          "       bottom-row alien (octopus), frame 0
//   'o'  ALIEN_C_FRAME1          "       bottom-row alien, frame 1
//   '*'  ALIEN_EXPLOSION         "       alien death poof
//   '#'  PLAYER_EXPLOSION        "       player death animation
//   '@'  BUNKER_BLOCK            "       one destructible bunker cell
//   'U'  SAUCER                  "       mystery UFO
//   'v'  ALIEN_BULLET_SQUIGGLE   "       wiggling bomb
//   '|'  ALIEN_BULLET_PLUMB      "       straight bomb
//   '0'..'9'  FONT_DIGITS        "       score/wave digits
//   'A'..'Z'  FONT_CAPS          "       title / HUD text
//
//  SOUNDS (integer ids)
//  --------------------
//   0  SOUND_SHOOT          player fire
//   1  SOUND_ALIEN_DEATH    alien explodes
//   2  SOUND_PLAYER_DEATH   player explodes
//   3  SOUND_MARCH0..6      (ids 3..9) the march, stepped as the swarm
//                            speeds up; use SOUND_MARCH_BASE + step
//  10  SOUND_SAUCER         saucer flyby loop
//  11  SOUND_SAUCER_DEATH   saucer destroyed
//  12  SOUND_EXTRA_LIFE     bonus fanfare
//  13  SOUND_BUNKER_HIT     bullet chews a bunker cell
// ============================================================================

// If you split the sections above into real headers, include them here:
#include "inc/assets.h"
#include "inc/core.h"
#include "inc/platform.h"
#include "inc/entities.h"
#include "inc/game.h"

int main() {
    si::g_rng.seed(0x1234ABCD);   // or a real entropy source

    si::Game  game;
    si::Input input;
    si::Video video;

    // one-time GPU setup: no texture, sprites come from region ids alone
    video.init();

    for (;;) {
        // 1) simulate one frame; input.read() queries each button's
        //    SDK function directly (gamepad_up(), gamepad_button_a(), ...)
        game.runFrame(input);

        // 3) draw one frame (blit -> select_region + draw_region_at)
        game.draw(video);

        // 4) end-of-frame GPU sync: present the frame.
        video.sync();
    }
    return 0;
}
