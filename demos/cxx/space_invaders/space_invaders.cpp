// *****************************************************************************
//  SPACE INVADERS  —  an object-oriented Space Invaders for Vircon32,
//  written in the v32c++ subset and drawn entirely with glyphs of the BIOS
//  font (texture -1). No custom textures.
//
//  Build:  v32c++ -I inc -o obj/space_invaders.c space_invaders.cpp
//          (see the Makefile)
//
//  LAYOUT. v32c++ resolves and inlines .hpp includes itself, so the game
//  is one translation unit made of:
//
//      inc/assets.hpp     sprite ids, sound ids, shared constants
//      inc/core.hpp       Random, Vec2, Rect, the sine table
//      inc/platform.hpp   Input, Video, Sound -- thin wrappers over the SDK
//      inc/entities.hpp   Entity and Player, Bullet, Alien(s), Bunker, Saucer
//      inc/swarm.hpp      the 5 x 11 formation and its march
//      inc/text.hpp       numbers, text, the block-letter title
//      inc/game.hpp       Game: owns everything, runs a frame
//      space_invaders.cpp this file: cartridge hints and main()
//
//  WHAT IT SHOWS OFF. The game was first written when v32c++ had far less
//  of C++ than it has now, and was full of ways round what was missing.
//  This version uses the language as intended:
//
//    * Objects held BY VALUE as members (Game has a Player, a Swarm and a
//      Saucer, not pointers to them); their constructors run on their own.
//    * std::vector<Bullet> for the bombs and std::array<Bunker, 4> for the
//      shields -- v32c++'s built-in generics -- with range-based for, and
//      `mBombs.push_back(Bullet(...))` building the element in place.
//    * Structs passed and returned by value: Vec2 is two plain ints with
//      operators, Entity::bounds() returns a Rect.
//    * Pure virtual functions (Entity::update, Entity::draw), member
//      initializer lists for members that are objects, default member
//      initializers (`int mLives = 3;`), ternaries, switch, nullptr.
//    * Arrays sized by named constants and filled from initializer lists.
//    * The SDK's own names (color_green, ...) straight from its headers.
//
//  Still written around, because v32c++ does not have them: class-nested
//  enums and static members (both are namespace-level here instead).
//
//  CONTROLS. Left/right move, A or B fires, START pauses (the pause menu
//  sets the volumes and switches the gameplay music).
// *****************************************************************************

#title "[v32cxx] C++/OOP Space Invaders"
#version 1.0

// Sound hints: DECLARATION ORDER IS THE SOUND ID (0..17), and must match
// AssetIds::Sounds in inc/assets.hpp. The macro names are WAV_* rather
// than SOUND_* on purpose: v32c++ emits `#define NAME id` for each hint,
// and a SOUND_SHOOT macro would rewrite the enum entry of the same name.
#sound WAV_SHOOT        "sounds/shoot.wav"
#sound WAV_ALIEN_DEATH  "sounds/alien_death.wav"
#sound WAV_PLAYER_DEATH "sounds/player_death.wav"
#sound WAV_MARCH0       "sounds/march0.wav"
#sound WAV_MARCH1       "sounds/march1.wav"
#sound WAV_MARCH2       "sounds/march2.wav"
#sound WAV_MARCH3       "sounds/march3.wav"
#sound WAV_MARCH4       "sounds/march4.wav"
#sound WAV_MARCH5       "sounds/march5.wav"
#sound WAV_MARCH6       "sounds/march6.wav"
#sound WAV_SAUCER       "sounds/saucer.wav"
#sound WAV_SAUCER_DEATH "sounds/saucer_death.wav"
#sound WAV_EXTRA_LIFE   "sounds/extra_life.wav"
#sound WAV_BUNKER_HIT   "sounds/bunker_hit.wav"
#sound WAV_MENU_MOVE    "sounds/menu_move.wav"
#sound WAV_MENU_SELECT  "sounds/menu_select.wav"
#sound WAV_MUSIC_TITLE  "sounds/music_title.wav"
#sound WAV_MUSIC_GAME   "sounds/music_game.wav"

#include "game.hpp"

int main() {
    si::g_rng.seed(0x1234ABCD);

    si::Game  game;
    si::Input input;
    si::Video video;
    video.init();

    for (;;) {
        game.runFrame(input);   // simulate
        game.draw(video);       // draw
        video.sync();           // wait for the next frame
    }
    return 0;
}
