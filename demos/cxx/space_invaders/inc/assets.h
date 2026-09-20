namespace si {

// ---------------------------------------------------------------------------
// Asset id tables -- the single place to renumber for your platform
// ---------------------------------------------------------------------------
namespace AssetIds {
    enum Sprites {
        PLAYER_SHIP           = '=',
        PLAYER_BULLET         = '!',
        ALIEN_A_FRAME0        = 'W',
        ALIEN_A_FRAME1        = 'w',
        ALIEN_B_FRAME0        = 'X',
        ALIEN_B_FRAME1        = 'x',
        ALIEN_C_FRAME0        = 'O',
        ALIEN_C_FRAME1        = 'o',
        ALIEN_EXPLOSION       = '*',
        PLAYER_EXPLOSION      = '#',
        BUNKER_BLOCK          = '@',
        SAUCER                = 'U',
        ALIEN_BULLET_SQUIGGLE = 'v',
        ALIEN_BULLET_PLUMB    = '|',
        FONT_DIGITS_BASE      = '0',   // + 0..9
        FONT_CAPS_BASE        = 'A'    // + 0..25
    };

    enum Sounds {
        SOUND_SHOOT         = 0,
        SOUND_ALIEN_DEATH   = 1,
        SOUND_PLAYER_DEATH  = 2,
        SOUND_MARCH_BASE    = 3,   // +0..+6 as swarm speeds up
        SOUND_SAUCER        = 10,
        SOUND_SAUCER_DEATH  = 11,
        SOUND_EXTRA_LIFE    = 12,
        SOUND_BUNKER_HIT    = 13
    };
}

// ---------------------------------------------------------------------------
// Shared game constants. An enum, NOT 'const int': these are used as array
// dimensions, and a C 'const int' is not a constant expression downstream.
// ---------------------------------------------------------------------------
enum GameConsts {
    SPRITE_W                 = 10,
    SPRITE_H                 = 20,
    PLAYFIELD_W              = 224,
    PLAYFIELD_H              = 256,

    PLAYER_WIDTH             = SPRITE_W,
    PLAYER_HEIGHT            = SPRITE_H,
    PLAYER_SPEED             = 2,
    PLAYER_RESPAWN_FRAMES    = 90,
    PLAYER_DEATH_FRAMES      = 40,
    PLAYER_HOME_Y            = 232,

    ALIEN_WIDTH              = SPRITE_W,
    ALIEN_HEIGHT             = SPRITE_H,
    ALIEN_EXPLOSION_FRAMES   = 12,

    SAUCER_WIDTH             = SPRITE_W,
    SAUCER_HEIGHT            = SPRITE_H,
    SAUCER_SPEED             = 1,

    BUNKER_CELLS_W           = 3,   // 3 * 10 = 30 px wide
    BUNKER_CELLS_H           = 2,   // 2 * 20 = 40 px tall
    BUNKER_CELL_W            = SPRITE_W,
    BUNKER_CELL_H            = SPRITE_H,

    SWARM_COLS               = 11,
    SWARM_ROWS               = 5,
    SWARM_GAP_X              = 20,  // 10px sprite + 10px spacing
    SWARM_GAP_Y              = 24,  // 20px sprite + 4px spacing

    BOMB_CAPACITY            = 8,
    BUNKER_COUNT             = 4
};

} // namespace si
