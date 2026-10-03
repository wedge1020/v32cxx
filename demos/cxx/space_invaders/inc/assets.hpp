#pragma once
// *****************************************************************************
//  assets.hpp — sprite ids, sound ids, and the game's shared constants
//
//  SPRITES are glyphs of the BIOS font: all 10 x 20 pixels, and a sprite's
//  id is simply the character it shows (texture -1 has one region per
//  character code). Nothing here is a custom texture.
//
//   '^' '_'   player cannon: turret glyph superimposed on the base glyph,
//             drawn at twice the size (the spare-life icons stay 1x)
//   '!'       player shot
//   'W' 'w'   top-row alien (squid), frames 0 and 1
//   'X' 'x'   middle-row alien (crab)
//   'O' 'o'   bottom-row alien (octopus)
//   '*'       alien explosion           '#'  player explosion
//   0x11..0x14  bunker cell, 1..4 hit points left (lightest..solid)
//   'v' '|'   alien bombs (squiggle, plumb)
//   the mystery saucer is assembled from 'O', 'o' and '|' -- see Saucer
//   'D' 'T' 'M' 'B' 'R' 'S'  falling power-up capsules -- see PowerUp
//   '0'..'9', 'A'..'Z'  score and text
//
//  SOUNDS are cartridge sound ids. The #sound hints in space_invaders.cpp
//  assign them in declaration order, which must match the Sounds enum.
// *****************************************************************************

namespace si {

namespace AssetIds {
    enum Sprites {
        PLAYER_SHIP_TURRET     = '^',   // superimposed on PLAYER_SHIP_BASE
        PLAYER_SHIP_BASE       = '_',   //   (both drawn in the same cell)
        PLAYER_BULLET          = '!',
        ALIEN_A_FRAME0         = 'W',
        ALIEN_A_FRAME1         = 'w',
        ALIEN_B_FRAME0         = 'X',
        ALIEN_B_FRAME1         = 'x',
        ALIEN_C_FRAME0         = 'O',
        ALIEN_C_FRAME1         = 'o',
        ALIEN_EXPLOSION        = '*',
        PLAYER_EXPLOSION       = '#',
        BUNKER_BLOCK_1         = 0x11,  // most damaged bunker cell
        BUNKER_BLOCK_2         = 0x12,
        BUNKER_BLOCK_3         = 0x13,
        BUNKER_BLOCK_4         = 0x14,  // undamaged (solid) bunker cell
        SAUCER_HULL            = 'O',   // stretched wide: the saucer's body
        SAUCER_DOME            = 'o',   // stretched, centered on top of it
        SAUCER_LIGHT           = '|',   // rim lights sliding round the hull
        ALIEN_BULLET_SQUIGGLE  = 'v',
        ALIEN_BULLET_PLUMB     = '|',
        PLAYER_SHIELD          = 'O'    // stretched into a bubble round the cannon
    };

    enum Sounds {
        SOUND_SHOOT         = 0,
        SOUND_ALIEN_DEATH   = 1,
        SOUND_PLAYER_DEATH  = 2,
        SOUND_MARCH_BASE    = 3,   // +0..+6 as the swarm speeds up
        SOUND_SAUCER        = 10,  // looping warble while it flies
        SOUND_SAUCER_DEATH  = 11,
        SOUND_EXTRA_LIFE    = 12,
        SOUND_BUNKER_HIT    = 13,
        SOUND_MENU_MOVE     = 14,
        SOUND_MENU_SELECT   = 15,
        SOUND_MUSIC_TITLE   = 16,  // looping title screen theme
        SOUND_MUSIC_GAME    = 17   // looping gameplay theme
    };
}

// An enum rather than `const int`: these size arrays (int mCells[...],
// Alien* mGrid[SWARM_ROWS][SWARM_COLS], std::array<Bunker, BUNKER_COUNT>),
// and a C `const int` is not a constant expression in the generated code.
enum GameConsts {
    SPRITE_W                 = 10,
    SPRITE_H                 = 20,
    PLAYFIELD_W              = 448,  // 448 * 45/32 = 630 of the 640 px used
    PLAYFIELD_H              = 256,

    // The cannon is drawn at twice glyph size: a 20 x 40 cell whose
    // bottom edge is the bottom of the playfield.
    PLAYER_SCALE             = 2,
    PLAYER_WIDTH             = SPRITE_W * PLAYER_SCALE,
    PLAYER_HEIGHT            = SPRITE_H * PLAYER_SCALE,
    PLAYER_SPEED             = 2,
    PLAYER_RESPAWN_FRAMES    = 90,
    PLAYER_DEATH_FRAMES      = 40,
    PLAYER_HOME_Y            = PLAYFIELD_H - PLAYER_HEIGHT,
    INVASION_Y               = 232,  // aliens reaching this line end the game

    // The visible glyphs are much smaller than their cells, so the thin
    // ones get an inset hitbox (Player::bounds, Bullet::bounds); full-cell
    // boxes made bombs "hit" while still 10-30 px away. The cannon's
    // turret+base band is roughly rows 12..28 of its 40-row cell.
    PLAYER_HIT_INSET_X       = 2,
    PLAYER_HIT_TOP           = 12,
    PLAYER_HIT_HEIGHT        = 16,
    BULLET_HIT_INSET_X       = 3,
    BULLET_HIT_INSET_Y       = 5,

    // how far the '^' turret drops toward the '_' base when the two are
    // superimposed (they sit in different parts of the font cell), in
    // glyph pixels: doubled for the 2x cannon
    PLAYER_TURRET_DROP       = 9,

    // Weapons and power-ups
    SHOT_SPEED               = 8,    // px per frame, upward
    MEGA_SHOT_SPEED          = 6,
    MEGA_SHOT_ENERGY         = 6,    // targets it destroys before it is spent:
                                     // a full column of 5, and one to spare
    MEGA_SHOT_WIDTH          = 20,   // its hitbox: one alien column wide
    MEGA_RECHARGE_FRAMES     = 60,   // wait after a mega shot before the next
    DOUBLE_SHOT_SPREAD       = 6,    // each of the two shots, px from center
    TRIPLE_SHOT_DRIFT        = 2,    // the outer two shots' sideways px per frame
    POWERUP_FALL_SPEED       = 1,
    POWERUP_CAPACITY         = 4,    // most capsules falling at once
    POWERUP_ODDS             = 10,   // one alien in this many drops a capsule

    ALIEN_WIDTH              = SPRITE_W,
    ALIEN_HEIGHT             = SPRITE_H,
    ALIEN_EXPLOSION_FRAMES   = 12,

    // The saucer: a 40 x 16 box. The hull is an 'O' stretched to the full
    // 40 px; the dome an 'o' stretched to 20 px, centered above it.
    SAUCER_WIDTH             = 40,
    SAUCER_HEIGHT            = 16,
    SAUCER_SPEED             = 1,
    SAUCER_Y                 = 18,   // clear of the score line above it
    SAUCER_LIGHTS            = 4,    // rim lights (half are behind the hull)
    SAUCER_SPIN_FRAMES       = 4,    // frames per 1/16 turn of the lights

    // umbrella-shaped bunker: a 6x3 grid of 10x20 cells (60x60 px); only
    // the cells inside the classic outline exist (Bunker::shapeHas)
    BUNKER_CELLS_W           = 6,
    BUNKER_CELLS_H           = 3,
    BUNKER_CELL_W            = SPRITE_W,
    BUNKER_CELL_H            = SPRITE_H,
    BUNKER_CELL_HP           = 4,
    BUNKER_Y                 = 168,

    SWARM_COLS               = 11,
    SWARM_ROWS               = 5,
    SWARM_GAP_X              = 20,   // 10px sprite + 10px spacing
    SWARM_GAP_Y              = 19,   // 20px sprite, rows nearly touching

    BOMB_CAPACITY            = 8,    // most alien bombs in flight at once
    BUNKER_COUNT             = 4
};

} // namespace si
