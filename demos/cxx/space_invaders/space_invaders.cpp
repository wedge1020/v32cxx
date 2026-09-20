#include "video.h"
#include "audio.h"
#include "input.h"
#include "misc.h"   // rand()/srand() (hardware RNG), malloc/free
#include "time.h"   // end_frame()

#title "[v32cxx] C++/OOP Space Invaders"
#version 1.0

// ============================================================================
//  SPACE INVADERS - portable object-oriented C++ skeleton
//
//  Freestanding: NO standard library headers are used. Everything needed
//  beyond core C++ (random numbers, containers) is implemented here.
//  Written to the v32c++ subset: no templates, no static members, no
//  'explicit', no in-class default member initializers, no class-nested
//  enums, no 'unsigned', no ternaries, and NO bare constructor-call /
//  functional-cast expressions ("Vec2(x, y)"). Only `new T(args)` may
//  construct with arguments in an expression; everywhere else uses local
//  declarations or int arguments. Default parameter values ARE now used
//  (v32c++ implements them: fill_default_args splices cloned defaults
//  into calls that omit trailing arguments).
//
//  Vec2 is deliberately a ONE-WORD type (a single int packing x/y as two
//  16-bit halves): the Vircon32 C compiler only accepts parameters and
//  return values of exactly one word, so a 2-int Vec2 returned by value
//  (operator+ etc.) would be rejected downstream. Packing keeps natural
//  by-value math while staying one word. Coordinates stay well inside
//  the 16-bit range (playfield is 224 x 256).
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
//   '^'  PLAYER_SHIP_TURRET      "       player cannon turret (upper glyph)
//   '_'  PLAYER_SHIP_BASE        "       player cannon base (lower glyph);
//                                        drawn superimposed on the turret
//   '!'  PLAYER_BULLET           "       player shot
//   'W'  ALIEN_A_FRAME0          "       top-row alien (squid), frame 0
//   'w'  ALIEN_A_FRAME1          "       top-row alien, frame 1
//   'X'  ALIEN_B_FRAME0          "       middle-row alien (crab), frame 0
//   'x'  ALIEN_B_FRAME1          "       middle-row alien, frame 1
//   'O'  ALIEN_C_FRAME0          "       bottom-row alien (octopus), frame 0
//   'o'  ALIEN_C_FRAME1          "       bottom-row alien, frame 1
//   '*'  ALIEN_EXPLOSION         "       alien death poof
//   '#'  PLAYER_EXPLOSION        "       player death animation
//   0x11 BUNKER_BLOCK_1         "       bunker cell, 1 hit point left (lightest)
//   0x12 BUNKER_BLOCK_2         "       bunker cell, 2 hit points left
//   0x13 BUNKER_BLOCK_3         "       bunker cell, 3 hit points left
//   0x14 BUNKER_BLOCK_4         "       bunker cell, undamaged (solid)
//   'U'  SAUCER                  "       mystery UFO
//   'v'  ALIEN_BULLET_SQUIGGLE   "       wiggling bomb
//   '|'  ALIEN_BULLET_PLUMB      "       straight bomb
//   '0'..'9'  FONT_DIGITS        "       score/wave digits
//   'A'..'Z'  FONT_CAPS          "       title / HUD text
//
//  COLLISION HITBOXES
//  ------------------
//  The visible glyphs are much smaller than their 10x20 font cells, so
//  full-cell hitboxes made bombs "hit" the player while still 10-30 px
//  away on screen. Entity::getBounds is therefore now VIRTUAL, and the
//  thin-glyph classes (Player, Bullet) override it with an INSET,
//  cell-centered hitbox while keeping mPos as the CELL position -- draw
//  coordinates, clamping, respawn and muzzle math all stay unchanged.
//  Tune BULLET_HIT_INSET_X/Y and PLAYER_HIT_INSET_Y to taste.
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

// === FILE: si_assets.h ===
namespace si {

// ---------------------------------------------------------------------------
// Asset id tables -- the single place to renumber for your platform
// ---------------------------------------------------------------------------
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
        SAUCER                 = 'U',
        ALIEN_BULLET_SQUIGGLE  = 'v',
        ALIEN_BULLET_PLUMB     = '|',
        FONT_DIGITS_BASE       = '0',   // + 0..9
        FONT_CAPS_BASE         = 'A'    // + 0..25
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
    PLAYFIELD_W              = 448,  // wide: 448*45/32 = 630 of 640 px used
    PLAYFIELD_H              = 256,

    PLAYER_WIDTH             = SPRITE_W,
    PLAYER_HEIGHT            = SPRITE_H,
    PLAYER_SPEED             = 2,
    PLAYER_RESPAWN_FRAMES    = 90,
    PLAYER_DEATH_FRAMES      = 40,
    PLAYER_HOME_Y            = 232,

    // player hitbox inset (turret+base glyph band, cell-centered):
    // '^' sits around mid-cell, '_' at the bottom, so the visible ship
    // spans roughly the lower 8 px of the 20 px cell
    PLAYER_HIT_INSET_Y       = 6,

    // how far the '^' turret glyph drops toward the '_' base when the
    // two are superimposed (they sit in different parts of the 10x20
    // font cell; without this the caret hovers well above the base)
    PLAYER_TURRET_DROP       = 9,

    // projectile hitbox insets ('|' is a ~2 px stroke, 'v' a small
    // chevron -- both far smaller than their 10x20 cell)
    BULLET_HIT_INSET_X       = 3,
    BULLET_HIT_INSET_Y       = 5,

    ALIEN_WIDTH              = SPRITE_W,
    ALIEN_HEIGHT             = SPRITE_H,
    ALIEN_EXPLOSION_FRAMES   = 12,

    SAUCER_WIDTH             = SPRITE_W,
    SAUCER_HEIGHT            = SPRITE_H,
    SAUCER_SPEED             = 1,

    // umbrella-shaped bunker: 6x3 grid of 10x20 cells (60x60 px), but
    // only the cells inside the classic shape exist (see Bunker::ctor):
    // chamfered top corners, solid mid, legs with an arch underneath
    BUNKER_CELLS_W           = 6,
    BUNKER_CELLS_H           = 3,
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
// === END FILE: si_assets.h ===


// === FILE: si_core.h ===  (freestanding replacements for library facilities)
namespace si {

// ---------------------------------------------------------------------------
// Random: small deterministic LCG, replaces <cstdlib> rand().
// Plain 'int' state (no 'unsigned' in the subset).
// ---------------------------------------------------------------------------
class Random {
public:
    // thin wrapper over the hardware RNG in misc.h (rand/srand);
    // note srand(0) is ignored by the hardware (0 never set as seed)
    void seed(int s) { srand(s); }

    // returns 0..limit-1; default limit exercises v32c++'s default
    // parameter values (fill_default_args splices the literal in at
    // every call site that omits it)
    int next(int limit = 2) {
        int r = rand();          // full 32-bit value, may be negative
        if (r < 0) r = -r;
        return r % limit;
    }

    // returns 0 or 1
    int bit() { return next(); }
};

// ---------------------------------------------------------------------------
// Vec2: operator overloading used pervasively through the game.
// ONE WORD (a single int): x in the high 16 bits, y in the low 16, so it
// can be passed and returned BY VALUE under Vircon32 C's one-word rule.
// ---------------------------------------------------------------------------
class Vec2 {
public:
    Vec2() : mV(0) {}
    Vec2(int px, int py) {
        mV = ((px & 0xFFFF) << 16) | (py & 0xFFFF);
    }

    // SIGN-EXTENSION WITHOUT '>>' ON A NEGATIVE VALUE: Vircon32's
    // shift-right does not sign-extend (logical shift), so reading the
    // high half as 'mV >> 16' returned ~65534 for a negative x -- the
    // player's left-edge clamp (< 0) never fired, the right-edge clamp
    // caught the huge positive instead, and the player "wrapped" from
    // the left edge to the right edge. Both halves are now decoded with
    // only masks, a 1-bit shift of a MASKED value, and signed compares
    // (which the hardware does correctly): grab the magnitude bits, then
    // subtract 32768 when the half's own sign bit is set.
    int x() const {
        int h = (mV >> 16) & 0x7FFF;     // bits 30..16, shift-semantics-agnostic
        if (mV < 0) return h - 32768;    // bit 31 set -> negative x
        return h;
    }
    int y() const {
        int l = mV & 0x7FFF;             // bits 14..0
        if ((mV & 0x8000) != 0) return l - 32768;   // bit 15 set -> negative y
        return l;
    }

    void setX(int px) { mV = (mV & 0xFFFF) | ((px & 0xFFFF) << 16); }
    void setY(int py) { mV = (mV & (0xFFFF << 16)) | (py & 0xFFFF); }

    Vec2  operator+(const Vec2& o) const {
        Vec2 r(x() + o.x(), y() + o.y());
        return r;
    }
    Vec2  operator-(const Vec2& o) const {
        Vec2 r(x() - o.x(), y() - o.y());
        return r;
    }
    Vec2& operator+=(const Vec2& o) {
        setX(x() + o.x());
        setY(y() + o.y());
        return *this;
    }
    Vec2& operator-=(const Vec2& o) {
        setX(x() - o.x());
        setY(y() - o.y());
        return *this;
    }
    Vec2  operator*(int s) const {
        Vec2 r(x() * s, y() * s);
        return r;
    }
    bool  operator==(const Vec2& o) const { return mV == o.mV; }
    bool  operator!=(const Vec2& o) const { return mV != o.mV; }

private:
    int mV;
};

// ---------------------------------------------------------------------------
// Axis-aligned rectangle for collision. Plain int fields (no Vec2 member),
// never passed or returned by value except through out-parameters.
// ---------------------------------------------------------------------------
struct Rect {
    int x, y, w, h;

    Rect() : x(0), y(0), w(0), h(0) {}

    bool intersects(const Rect& o) const {
        return x < o.x + o.w && x + w > o.x &&
               y < o.y + o.h && y + h > o.y;
    }
};

// ---------------------------------------------------------------------------
// Entity lifecycle states (namespace-level: the subset has no class enums)
// ---------------------------------------------------------------------------
enum EntityState {
    STATE_ALIVE,
    STATE_DYING,
    STATE_DEAD
};

// ---------------------------------------------------------------------------
// BombList: minimal freestanding replacement for std::vector<Bullet*>.
// NOTE: intentionally NOT a template -- the target compiler has no template
// support, so this is a concrete class. Defined in si_game.h, AFTER Bullet's
// full definition: the v32c++ grammar has no class forward declarations.
// ---------------------------------------------------------------------------

// Shared RNG (namespace-level global: the subset has no static data members)
Random g_rng;

} // namespace si
// === END FILE: si_core.h ===


// === FILE: si_platform.h ===  (the three platform layers, Vircon32-wired)
namespace si {

// ---------------------------------------------------------------------------
// Input: per-button signed reads, never zero.
//
// Wired directly to Vircon32's individual per-button query functions:
// gamepad_up(), gamepad_down(), gamepad_left(), gamepad_right(),
// gamepad_button_start(), gamepad_button_a(), gamepad_button_b(),
// gamepad_button_x(), gamepad_button_y(), gamepad_button_l(),
// gamepad_button_r(). Each already returns the signed frame count
// (+N held for N frames / -N released for N frames), so read() just
// forwards it -- no polling state, no counters, no per-frame setup.
// ---------------------------------------------------------------------------
enum Button {
    BTN_UP = 0, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
    BTN_START, BTN_A, BTN_B, BTN_X, BTN_Y, BTN_L, BTN_R,
    BUTTON_COUNT
};

class Input {
public:
    // Query one button individually, e.g. read(BTN_A).
    // Returns a value that is NEVER 0:
    //   > 0 : pressed, for that many consecutive frames
    //   < 0 : not pressed, for that many consecutive frames
    int read(Button b) const {
        if (b == BTN_UP)    return gamepad_up();
        if (b == BTN_DOWN)  return gamepad_down();
        if (b == BTN_LEFT)  return gamepad_left();
        if (b == BTN_RIGHT) return gamepad_right();
        if (b == BTN_START) return gamepad_button_start();
        if (b == BTN_A)     return gamepad_button_a();
        if (b == BTN_B)     return gamepad_button_b();
        if (b == BTN_X)     return gamepad_button_x();
        if (b == BTN_Y)     return gamepad_button_y();
        if (b == BTN_L)     return gamepad_button_l();
        return gamepad_button_r();
    }
};

// ---------------------------------------------------------------------------
// Video: GPU-facing layer, wired to the Vircon32 SDK.
//   init()  -- one-time setup: select texture -1 (no texture; all sprites
//              come from region ids alone).
//   blit()  -- select_region(spriteId); draw_region_at(x, y);
//   clear() -- clear_screen(...) in preparation for the frame.
//   sync()  -- end_frame(): vsync / present. Call once per frame after all
//              drawing is done. Lives on Video, not Input.
// ---------------------------------------------------------------------------
class Video {
public:
    void init() {
        select_texture(-1);
        // Vircon32's screen is 640x360; the logical playfield is 448x256.
        // Scale by 360/256 = 1.40625 so the playfield fills the screen
        // height: 448 * 1.40625 = 630, and (640 - 630) / 2 = 5 pixels
        // of left margin -- nearly the full screen width is in play.
        set_drawing_scale(1.40625, 1.40625);
    }

    // Blit one sprite (10x20) with the given ASCII-value sprite id.
    // Logical (448x256) coordinates are mapped to the 640x360 screen:
    // 1.40625 == 45/32 exactly, done in integer math per call.
    void blit(int spriteId, int x, int y) {
        select_region(spriteId);
        draw_region_zoomed_at(5 + (x * 45) / 32, (y * 45) / 32);
    }

    // Blit TWO sprites superimposed in the same cell -- for multi-glyph
    // assemblies like the player cannon ('^' turret over '_' base).
    // dyA (default parameter value) drops glyph A that many pixels
    // toward glyph B, so glyphs living in different parts of the cell
    // can be pulled together visually.
    void blit2(int spriteIdA, int spriteIdB, int x, int y, int dyA = 0) {
        select_region(spriteIdA);
        draw_region_zoomed_at(5 + (x * 45) / 32, ((y + dyA) * 45) / 32);
        select_region(spriteIdB);
        draw_region_zoomed_at(5 + (x * 45) / 32, (y * 45) / 32);
    }

    // Clear the framebuffer.
    void clear() {
        clear_screen(color_black);
    }

    // Signal end of processing for this frame.
    void sync() {
        end_frame();
    }

    // Tint all subsequent region draws (GPU multiply color). Colors are
    // ABGR ints (see video.h); set_multiply_color is an SDK free function
    // passed through to the generated C like select_region. White
    // (0xFFFFFFFF) is neutral. clear_screen() is NOT affected by it.
    // NOTE: numeric literals, not the color_* macros -- those #defines
    // live in video.h, which v32c++ passes through without expanding.
    void tint(int color) {
        set_multiply_color(color);
    }
};

// ---------------------------------------------------------------------------
// Sound: stubbed playback of integer sound ids.
// Wire to the audio.h API (select_sound / play_sound / stop_sound) with a
// #sound cart hint per effect when you have audio assets ready.
// ---------------------------------------------------------------------------
class Sound {
public:
    void play(int soundId) {
        // TODO: select_sound(soundId); play_sound(soundId);
    }
    void stop(int soundId) {
        // TODO: stop_sound(soundId);
    }
};

} // namespace si
// === END FILE: si_platform.h ===


// === FILE: si_entities.h ===
namespace si {

// ---------------------------------------------------------------------------
// Entity base class: single inheritance hierarchy below this
// ---------------------------------------------------------------------------
class Entity {
public:
    Entity(int px, int py, int w, int h)
        : mW(w), mH(h), mState(STATE_ALIVE), mTimer(0) {
        mPos.setX(px);
        mPos.setY(py);
    }
    virtual ~Entity() {}

    // NOTE: the v32c++ grammar has no pure-virtual ('= 0') syntax, so these
    // base-class virtuals get empty bodies instead. Entity is never
    // instantiated directly, so the empty implementations are never called.
    virtual void update() {}                // advance one frame of simulation
    virtual void draw(Video& video) {}      // platform blit via Video

    int posX() const { return mPos.x(); }
    int posY() const { return mPos.y(); }
    void move(int dx, int dy) {
        mPos.setX(mPos.x() + dx);
        mPos.setY(mPos.y() + dy);
    }

    // bounds via out-parameter: Rect is bigger than one word, so it must
    // not be returned by value under Vircon32 C's one-word rule.
    // VIRTUAL now: thin-glyph subclasses (Player, Bullet) override it with
    // an inset, cell-centered hitbox so collision matches what's actually
    // visible on screen; mPos stays the CELL position in every class, so
    // drawing / clamping / respawn math is untouched by the insets.
    virtual void getBounds(Rect& r) const {
        r.x = mPos.x();
        r.y = mPos.y();
        r.w = mW;
        r.h = mH;
    }

    EntityState state() const { return mState; }
    void kill() { mState = STATE_DEAD; }

    // function overloading: same behavior, different parameter sets
    bool collidesWith(const Rect& r) const {
        Rect b;
        getBounds(b);
        return b.intersects(r);
    }
    bool collidesWith(const Entity& e) const {
        Rect b;
        Rect o;
        getBounds(b);
        e.getBounds(o);
        return b.intersects(o);
    }
    bool collidesWith(int px, int py) const {
        return px >= mPos.x() && px < mPos.x() + mW &&
               py >= mPos.y() && py < mPos.y() + mH;
    }

protected:
    Vec2        mPos;
    int         mW, mH;
    EntityState mState;
    int         mTimer;   // frames spent in DYING etc.

    void startDying(int frames) { mState = STATE_DYING; mTimer = frames; }
    void tickDeath() {
        if (mState == STATE_DYING) {
            --mTimer;
            if (mTimer <= 0) mState = STATE_DEAD;
        }
    }
};

// ---------------------------------------------------------------------------
// Player cannon
// ---------------------------------------------------------------------------
class Player : public Entity {
public:
    // ONE constructor with DEFAULT PARAMETER VALUES (now supported by
    // v32c++) replaces the old default/ two-ctor pair. Game constructs
    // with `new Player()` and immediately reset()s, exactly as before.
    // NOTE: Game cannot mention mPlayer in its initializer list (sema
    // rejects member inits for class-typed fields), hence this shape.
    Player(int px = 0, int py = PLAYER_HOME_Y)
        : Entity(px, py, PLAYER_WIDTH, PLAYER_HEIGHT),
          mLives(3), mRespawn(0), mWantsFire(false) {}

    // replaces 'mPlayer = Player(...)' -- assigning a whole Player
    // temporary would move more than one word by value
    void reset(int px, int py) {
        mPos.setX(px);
        mPos.setY(py);
        mLives = 3;
        mRespawn = 0;
        mWantsFire = false;
        mState = STATE_ALIVE;
        mTimer = 0;
    }

    // inset hitbox: the visible turret+base band spans roughly the lower
    // 8 px of the 20 px cell -- full-cell bounds made bombs connect
    // 10-30 px "early" (i.e. while the glyphs were still clearly apart)
    void getBounds(Rect& r) const {
        r.x = mPos.x();
        r.y = mPos.y() + PLAYER_HIT_INSET_Y;
        r.w = PLAYER_WIDTH;
        r.h = PLAYER_HEIGHT - 2 * PLAYER_HIT_INSET_Y;
    }

    void handleInput(const Input& in) {
        if (mState != STATE_ALIVE) return;
        if (in.read(BTN_LEFT) > 0)
            mPos.setX(mPos.x() - PLAYER_SPEED);
        if (in.read(BTN_RIGHT) > 0)
            mPos.setX(mPos.x() + PLAYER_SPEED);
        if (mPos.x() < 0) mPos.setX(0);
        if (mPos.x() > PLAYFIELD_W - PLAYER_WIDTH)
            mPos.setX(PLAYFIELD_W - PLAYER_WIDTH);
    }

    void update() {
        if (mState == STATE_DYING) {
            tickDeath();
            if (mState == STATE_DEAD) mRespawn = PLAYER_RESPAWN_FRAMES;
        } else if (mState == STATE_DEAD && mLives > 0) {
            --mRespawn;
            if (mRespawn == 0) respawn();
        }
    }

    void draw(Video& video) {
        if (mState == STATE_ALIVE) {
            // classic green cannon
            video.tint(0xFF00FF00);   // color_green (ABGR)
            // '^' turret superimposed over '_' base, same 10x20 cell,
            // turret dropped a few px so it sits ON the base, not above it
            video.blit2(AssetIds::PLAYER_SHIP_TURRET,
                        AssetIds::PLAYER_SHIP_BASE,
                        mPos.x(), mPos.y(), PLAYER_TURRET_DROP);
        } else if (mState == STATE_DYING) {
            video.tint(0xFF0080FF);   // color_orange: hot death flash
            video.blit(AssetIds::PLAYER_EXPLOSION, mPos.x(), mPos.y()); // '#'
        }
    }

    void fire() {
        if (mState == STATE_ALIVE) mWantsFire = true;
    }

    bool wantsFire() const { return mWantsFire; }
    void clearFire()    { mWantsFire = false; }

    int muzzleX() const { return mPos.x() + PLAYER_WIDTH / 2; }
    int muzzleY() const { return mPos.y() - SPRITE_H; }

    void hit(Sound& sfx) {
        if (mState == STATE_ALIVE) {
            --mLives;
            startDying(PLAYER_DEATH_FRAMES);
            sfx.play(AssetIds::SOUND_PLAYER_DEATH);
        }
    }

    int  lives() const    { return mLives; }
    void awardLife()      { ++mLives; }
    bool gameOver() const { return mLives <= 0 && mState == STATE_DEAD; }

private:
    void respawn() {
        mPos.setY(PLAYER_HOME_Y);
        mState = STATE_ALIVE;
    }

    int  mLives;
    int  mRespawn;
    bool mWantsFire;
};

// ---------------------------------------------------------------------------
// Projectiles (player bullet, alien bombs). Velocity kept as a Vec2 member
// so update() can use the overloaded operator+=.
// ---------------------------------------------------------------------------
class Bullet : public Entity {
public:
    Bullet(int px, int py, int vx, int vy, int spriteId)
        : Entity(px, py, SPRITE_W, SPRITE_H), mSprite(spriteId) {
        // class-typed field: cannot appear in the member-init list
        // (sema limitation) -- set through setters in the body instead
        mVel.setX(vx);
        mVel.setY(vy);
    }

    // inset hitbox: '|' is a ~2 px stroke and 'v' a small chevron inside
    // a 10x20 cell -- full-cell bounds collided with things the visible
    // glyph hadn't reached yet
    void getBounds(Rect& r) const {
        r.x = mPos.x() + BULLET_HIT_INSET_X;
        r.y = mPos.y() + BULLET_HIT_INSET_Y;
        r.w = SPRITE_W - 2 * BULLET_HIT_INSET_X;
        r.h = SPRITE_H - 2 * BULLET_HIT_INSET_Y;
    }

    void update() {
        mPos += mVel;   // Vec2::operator+=
        if (mPos.y() < -SPRITE_H || mPos.y() > PLAYFIELD_H)
            mState = STATE_DEAD;
    }

    void draw(Video& video) {
        video.tint(0xFFFFFFFF);       // white: neutral tracer
        video.blit(mSprite, mPos.x(), mPos.y());   // '!' / 'v' / '|'
    }

private:
    Vec2 mVel;
    int  mSprite;
};

// ---------------------------------------------------------------------------
// Invaders: base Alien + three row types via single inheritance
// ---------------------------------------------------------------------------
class Alien : public Entity {
public:
    Alien(int px, int py, int points)
        : Entity(px, py, ALIEN_WIDTH, ALIEN_HEIGHT),
          mPoints(points), mFrame(0), mColor(0xFFFFFFFF) {}

    void update() {
        tickDeath(); // DYING -> DEAD after explosion frames
    }

    void draw(Video& video) {
        if (mState == STATE_DYING)
            video.tint(0xFF0080FF);          // orange explosion poof
        else
            video.tint(mColor);              // this row's rainbow color
        video.blit(spriteId(), mPos.x(), mPos.y());   // 10x20, ASCII id
    }

    // per-instance tint (ABGR multiply color), set by Swarm::spawn --
    // per-INSTANCE, not per-subclass: rows 1-2 share AlienMiddleRow and
    // rows 3-4 share AlienBottomRow, so the row classes can't own it.
    // The Atari-rainbow look, one hue per row:
    //   row 0 squid    magenta
    //   row 1 crab     orange
    //   row 2 crab     yellow
    //   row 3 octopus  green
    //   row 4 octopus  cyan
    void setColor(int c) { mColor = c; }

    int points() const { return mPoints; }
    int frame() const  { return mFrame; }
    void setFrame(int f) { mFrame = f; }   // Swarm drives the march animation

    void destroy(Sound& sfx) {
        if (mState == STATE_ALIVE) {
            startDying(ALIEN_EXPLOSION_FRAMES);
            sfx.play(AssetIds::SOUND_ALIEN_DEATH);
        }
    }

protected:
    // virtual: frame sprite id for this alien type (empty base body: the
    // v32c++ grammar has no pure-virtual '= 0' syntax)
    virtual int spriteForFrame(int frame) const { return AssetIds::ALIEN_A_FRAME0; }
    int spriteId() const {
        if (mState == STATE_DYING) return AssetIds::ALIEN_EXPLOSION;
        return spriteForFrame(mFrame);
    }

    int mPoints;
    int mFrame;
    int mColor;   // ABGR tint; white until Swarm::spawn assigns the row's
};

class AlienTopRow : public Alien {          // squid, 30 pts
public:
    AlienTopRow(int px, int py) : Alien(px, py, 30) {}
protected:
    int spriteForFrame(int f) const {
        if (f == 0) return AssetIds::ALIEN_A_FRAME0;
        return AssetIds::ALIEN_A_FRAME1;
    }
};

class AlienMiddleRow : public Alien {        // crab, 20 pts
public:
    AlienMiddleRow(int px, int py) : Alien(px, py, 20) {}
protected:
    int spriteForFrame(int f) const {
        if (f == 0) return AssetIds::ALIEN_B_FRAME0;
        return AssetIds::ALIEN_B_FRAME1;
    }
};

class AlienBottomRow : public Alien {        // octopus, 10 pts
public:
    AlienBottomRow(int px, int py) : Alien(px, py, 10) {}
protected:
    int spriteForFrame(int f) const {
        if (f == 0) return AssetIds::ALIEN_C_FRAME0;
        return AssetIds::ALIEN_C_FRAME1;
    }
};

// ---------------------------------------------------------------------------
// Destructible bunker: a small grid of 10x20 blocks
// ---------------------------------------------------------------------------
class Bunker : public Entity {
public:
    Bunker(int px, int py)
        : Entity(px, py, BUNKER_CELLS_W * BUNKER_CELL_W,
                          BUNKER_CELLS_H * BUNKER_CELL_H) {
        // classic umbrella shape, 6 cols x 3 rows:
        //   row 0:  . X X X X .     chamfered top corners
        //   row 1:  X X X X X X     solid middle
        //   row 2:  X X . . X X     legs with an arch underneath
        // Every EXISTING cell starts at 4 hit points; cells outside the
        // shape are permanently 0. (No array initializer lists -- dims
        // and contents stay plain statements per the subset's rules.)
        for (int cy = 0; cy < BUNKER_CELLS_H; ++cy) {
            for (int cx = 0; cx < BUNKER_CELLS_W; ++cx) {
                if (shapeHas(cx, cy)) mCells[cy * BUNKER_CELLS_W + cx] = 4;
                else                  mCells[cy * BUNKER_CELLS_W + cx] = 0;
            }
        }
    }

    // is (cx, cy) part of the umbrella outline?
    bool shapeHas(int cx, int cy) const {
        if (cy == 0) return cx > 0 && cx < BUNKER_CELLS_W - 1;
        if (cy == BUNKER_CELLS_H - 1)
            return cx < 2 || cx >= BUNKER_CELLS_W - 2;
        return true;
    }

    void update() {}

    void draw(Video& video) {
        video.tint(0xFF00FF00);   // green shields (classic)
        for (int cy = 0; cy < BUNKER_CELLS_H; ++cy)
            for (int cx = 0; cx < BUNKER_CELLS_W; ++cx)
                if (cell(cx, cy) > 0)
                    // hp 1..4 -> sprites 0x11..0x14 (light -> solid):
                    // BUNKER_BLOCK_1 + (hp - 1) == 0x10 + hp
                    video.blit(AssetIds::BUNKER_BLOCK_1 + cell(cx, cy) - 1,
                               mPos.x() + cx * BUNKER_CELL_W,
                               mPos.y() + cy * BUNKER_CELL_H);
    }

    // erode cells where the rect overlaps: each overlapping cell loses
    // 'dmg' hit points (difficulty-scaled enemy weapon strength);
    // returns true if anything chipped
    bool erode(const Rect& hit, Sound& sfx, int dmg) {
        bool any = false;
        for (int cy = 0; cy < BUNKER_CELLS_H; ++cy) {
            for (int cx = 0; cx < BUNKER_CELLS_W; ++cx) {
                if (cell(cx, cy) <= 0) continue;
                Rect r;
                r.x = mPos.x() + cx * BUNKER_CELL_W;
                r.y = mPos.y() + cy * BUNKER_CELL_H;
                r.w = BUNKER_CELL_W;
                r.h = BUNKER_CELL_H;
                if (r.intersects(hit)) {
                    int hp = cell(cx, cy) - dmg;
                    if (hp < 0) hp = 0;
                    setCell(cx, cy, hp);
                    any = true;
                }
            }
        }
        if (any) sfx.play(AssetIds::SOUND_BUNKER_HIT);
        return any;
    }

private:
    int cell(int x, int y) const {
        return mCells[y * BUNKER_CELLS_W + x];
    }
    void setCell(int x, int y, int v) {
        mCells[y * BUNKER_CELLS_W + x] = v;
    }
    int mCells[18];     // hit points per cell, 0..4. BUNKER_CELLS_W (6) *
                         // BUNKER_CELLS_H (3): dims must be INT_LITERALs
};

// ---------------------------------------------------------------------------
// Mystery saucer
// ---------------------------------------------------------------------------
class Saucer : public Entity {
public:
    // kill() in the body: Entity's ctor forces STATE_ALIVE, which made
    // the saucer fly from frame 0 instead of waiting out mCooldown.
    Saucer() : Entity(0, 16, SAUCER_WIDTH, SAUCER_HEIGHT),
               mDir(1), mCooldown(600) { kill(); }

    void update() {
        if (mState == STATE_ALIVE) {
            mPos.setX(mPos.x() + mDir * SAUCER_SPEED);
            if (mPos.x() < -SAUCER_WIDTH || mPos.x() > PLAYFIELD_W) {
                mState = STATE_DEAD;
                mCooldown = 500 + g_rng.next(300);
            }
        } else if (mState == STATE_DYING) {
            tickDeath();
        } else {
            --mCooldown;
            if (mCooldown <= 0) launch();
        }
    }

    void draw(Video& video) {
        if (mState != STATE_DEAD) {
            video.tint(0xFF0000FF);   // color_red: the mystery UFO
            video.blit(AssetIds::SAUCER, mPos.x(), mPos.y());   // 'U'
        }
    }

    void launch() {
        if (g_rng.bit()) mDir = 1;
        else             mDir = -1;
        if (mDir > 0) mPos.setX(-SAUCER_WIDTH);
        else          mPos.setX(PLAYFIELD_W);
        mState = STATE_ALIVE;
    }

    int scoreValue() const { return 50 + 10 * g_rng.next(8); } // 50..120

    void destroy(Sound& sfx) {
        if (mState == STATE_ALIVE) {
            startDying(ALIEN_EXPLOSION_FRAMES);
            sfx.play(AssetIds::SOUND_SAUCER_DEATH);
        }
    }

    bool flying() const { return mState == STATE_ALIVE; }

private:
    int mDir;
    int mCooldown;
};

} // namespace si
// === END FILE: si_entities.h ===


// === FILE: si_game.h ===
namespace si {

// ---------------------------------------------------------------------------
// Difficulty (namespace-level: the subset has no class enums). Chosen on the
// title screen, consulted by the Swarm's march interval and Game's bomb-spawn
// cadence. NOTE: declared BEFORE Swarm -- v32c++'s lexer only classifies an
// identifier as a type once it is registered in the symbol table, so a type
// used before its declaration parses as a bare IDENTIFIER and fails with
// "unexpected IDENTIFIER, expecting COLONCOLON" at Swarm::setDifficulty.
// ---------------------------------------------------------------------------
enum GameDifficulty {
    DIFF_EASY,
    DIFF_MEDIUM,
    DIFF_HARD      // the original tuning -- what the game played like
};

// ---------------------------------------------------------------------------
// BombList: minimal freestanding replacement for std::vector<Bullet*>.
// Lives here (after Bullet's full definition) because the v32c++ grammar
// has no class forward declarations.
// ---------------------------------------------------------------------------
class BombList {
public:
    BombList() : mSize(0) {
        for (int i = 0; i < BOMB_CAPACITY; ++i) mItems[i] = 0;
    }

    int  size() const   { return mSize; }
    bool empty() const  { return mSize == 0; }
    bool full() const   { return mSize >= BOMB_CAPACITY; }

    // NOTE: returns Bullet* by value, not Bullet*& -- the grammar's
    // pointer_opt allows a single '*' or '&', not both stacked. All uses
    // are reads, so by-value loses nothing.
    Bullet* operator[](int i) { return mItems[i]; }

    void push(Bullet* v) {
        if (mSize < BOMB_CAPACITY) {
            mItems[mSize] = v;
            ++mSize;
        }
    }

    // erase slot i; caller deletes the pointed-to Bullet first
    void eraseAt(int i) {
        for (int j = i + 1; j < mSize; ++j) mItems[j - 1] = mItems[j];
        --mSize;
        mItems[mSize] = 0;
    }

    void clear() { mSize = 0; }

private:
    Bullet* mItems[8];   // BOMB_CAPACITY: dims must be INT_LITERALs
    int     mSize;
};

// ---------------------------------------------------------------------------
// The Swarm: grid of Aliens with marching logic (a friend of Game)
// ---------------------------------------------------------------------------
class Swarm {
public:
    Swarm() : mDx(2), mAnimFrame(0), mStepCooldown(0), mDifficulty(DIFF_MEDIUM) {
        for (int r = 0; r < SWARM_ROWS; ++r)
            for (int c = 0; c < SWARM_COLS; ++c)
                mGrid[r][c] = 0;
    }

    void setDifficulty(GameDifficulty d) { mDifficulty = d; }

    void spawn(int baseY) {
        // center the 220px-wide swarm grid in the 448px playfield:
        // (448 - 11 * 20) / 2 = 114
        int x0 = (PLAYFIELD_W - SWARM_COLS * SWARM_GAP_X) / 2;
        for (int r = 0; r < SWARM_ROWS; ++r) {
            for (int c = 0; c < SWARM_COLS; ++c) {
                int px = x0 + c * SWARM_GAP_X;
                int py = baseY + r * SWARM_GAP_Y;
                // Atari rainbow: one hue per row (ABGR multiply colors)
                int rowColor[5];
                rowColor[0] = 0xFFFF00FF;   // magenta (squid)
                rowColor[1] = 0xFF0080FF;   // orange
                rowColor[2] = 0xFF00FFFF;   // yellow
                rowColor[3] = 0xFF00FF00;   // green
                rowColor[4] = 0xFFFFFF00;   // cyan
                Alien* a;
                if (r == 0)      a = new AlienTopRow(px, py);
                else if (r < 3)  a = new AlienMiddleRow(px, py);
                else             a = new AlienBottomRow(px, py);
                a->setColor(rowColor[r]);
                mGrid[r][c] = a;
            }
        }
    }

    ~Swarm() { destroyAll(); }
    void destroyAll() {
        for (int r = 0; r < SWARM_ROWS; ++r)
            for (int c = 0; c < SWARM_COLS; ++c)
                delete mGrid[r][c];
    }

    int aliveCount() const {
        int n = 0;
        for (int r = 0; r < SWARM_ROWS; ++r)
            for (int c = 0; c < SWARM_COLS; ++c)
                if (mGrid[r][c] && mGrid[r][c]->state() != STATE_DEAD) ++n;
        return n;
    }

    bool reachedBottom(int limitY) const {
        for (int r = SWARM_ROWS - 1; r >= 0; --r)
            for (int c = 0; c < SWARM_COLS; ++c)
                if (mGrid[r][c] && mGrid[r][c]->state() == STATE_ALIVE &&
                    mGrid[r][c]->posY() + ALIEN_HEIGHT >= limitY)
                    return true;
        return false;
    }

    // march one animation/exchange step; steps faster as aliens die.
    // Moves the ALIENS themselves (no separate offset): each alive alien
    // shifts by mDx, or drops one sprite height and reverses at the
    // edges. Extents are computed from the aliens' ACTUAL positions.
    void step(Sound& sfx) {
        if (mStepCooldown > 0) { --mStepCooldown; return; }
        mStepCooldown = stepInterval();

        // find horizontal extent of living aliens, from real positions
        int minX = PLAYFIELD_W, maxX = -1;
        for (int r = 0; r < SWARM_ROWS; ++r)
            for (int c = 0; c < SWARM_COLS; ++c)
                if (mGrid[r][c] && mGrid[r][c]->state() == STATE_ALIVE) {
                    if (mGrid[r][c]->posX() < minX) minX = mGrid[r][c]->posX();
                    if (mGrid[r][c]->posX() > maxX) maxX = mGrid[r][c]->posX();
                }
        if (maxX < 0) return; // nobody left

        bool drop = (mDx > 0 && maxX + mDx + ALIEN_WIDTH > PLAYFIELD_W) ||
                    (mDx < 0 && minX + mDx < 0);
        mAnimFrame = 1 - mAnimFrame;
        for (int r = 0; r < SWARM_ROWS; ++r)
            for (int c = 0; c < SWARM_COLS; ++c) {
                Alien* a = mGrid[r][c];
                if (a && a->state() == STATE_ALIVE) {
                    if (drop) a->move(0, SPRITE_H);   // one sprite height
                    else      a->move(mDx, 0);
                    a->setFrame(mAnimFrame);
                }
            }
        if (drop) mDx = -mDx;
        sfx.play(AssetIds::SOUND_MARCH_BASE + marchStep());
    }

    void update(Sound& sfx) {
        step(sfx);
        for (int r = 0; r < SWARM_ROWS; ++r)
            for (int c = 0; c < SWARM_COLS; ++c)
                if (mGrid[r][c]) mGrid[r][c]->update();
    }

    void draw(Video& video) {
        for (int r = 0; r < SWARM_ROWS; ++r)
            for (int c = 0; c < SWARM_COLS; ++c)
                if (mGrid[r][c] && mGrid[r][c]->state() != STATE_DEAD)
                    mGrid[r][c]->draw(video);
    }

    // pick a random living alien (lowest in its column) to drop a bomb from.
    // Counts the shooter CANDIDATES first (one per column that has any
    // living alien), then picks among exactly those -- the old version
    // picked in [0, aliveCount) but only ever decremented once per
    // column, so any pick >= the number of living columns fell through
    // the whole loop and returned 0 (no bomb that cycle, ~80% of the
    // time with a full swarm).
    Alien* randomShooter() {
        int shooters = 0;
        for (int c = 0; c < SWARM_COLS; ++c) {
            for (int r = SWARM_ROWS - 1; r >= 0; --r) {
                Alien* a = mGrid[r][c];
                if (a && a->state() == STATE_ALIVE) {
                    ++shooters;   // only the lowest alien per column shoots
                    break;
                }
            }
        }
        if (!shooters) return 0;
        int pick = g_rng.next(shooters);
        for (int c = 0; c < SWARM_COLS; ++c) {
            for (int r = SWARM_ROWS - 1; r >= 0; --r) {
                Alien* a = mGrid[r][c];
                if (a && a->state() == STATE_ALIVE) {
                    if (pick == 0) return a;
                    --pick;
                    break;
                }
            }
        }
        return 0;
    }

    // Collision helper the Game (a friend) drives
    Alien* hitTest(const Rect& bullet) {
        for (int r = 0; r < SWARM_ROWS; ++r)
            for (int c = 0; c < SWARM_COLS; ++c) {
                Alien* a = mGrid[r][c];
                if (a && a->state() == STATE_ALIVE && a->collidesWith(bullet))
                    return a;
            }
        return 0;
    }

private:
    int stepInterval() const {
        int n = aliveCount();
        int base;
        if (n > 40) base = 30;
        else if (n > 30) base = 24;
        else if (n > 20) base = 18;
        else if (n > 10) base = 12;
        else if (n >  5) base = 8;
        else if (n >  1) base = 4;
        else base = 2;
        // difficulty slows the march: more frames between steps
        if (mDifficulty == DIFF_EASY)   base += 12;
        if (mDifficulty == DIFF_MEDIUM) base += 6;
        // DIFF_HARD: the original tuning, unchanged
        return base;
    }
    int marchStep() const {
        int n = aliveCount();
        if (n > 40) return 0;
        if (n > 30) return 1;
        if (n > 20) return 2;
        if (n > 10) return 3;
        if (n >  5) return 4;
        if (n >  1) return 5;
        return 6;
    }

    Alien* mGrid[5][11];   // SWARM_ROWS x SWARM_COLS: dims must be INT_LITERALs
    int    mDx;
    int    mAnimFrame;
    int    mStepCooldown;
    GameDifficulty mDifficulty;

    friend class Game;   // Game may reach into the grid directly
};

// ---------------------------------------------------------------------------
// HUD text helpers (free functions: the subset has no static member
// functions, and 'ScoreDisplay::draw' would need them)
// ---------------------------------------------------------------------------
void drawNumber(Video& video, int value, int x, int y) {
    video.tint(0xFFFFFFFF);   // text always neutral white
    // render digits with FONT_DIGITS_BASE + digit ('0'..'9', 10x20)
    char buf[12];
    int n = 0;
    if (value == 0) { buf[0] = '0'; n = 1; }
    while (value > 0 && n < 11) {
        buf[n] = '0' + value % 10;
        ++n;
        value = value / 10;
    }
    for (int i = 0; i < n; ++i)
        video.blit(buf[n - 1 - i], x + i * SPRITE_W, y);
}

void drawText(Video& video, const char* text, int x, int y) {
    video.tint(0xFFFFFFFF);   // text always neutral white
    // render text using each character's ASCII value as the sprite id
    for (int i = 0; text[i] != 0; ++i)
        video.blit(text[i], x + i * SPRITE_W, y);
}

// ---------------------------------------------------------------------------
// Game states (namespace-level: the subset has no class enums)
// ---------------------------------------------------------------------------
enum GameState {
    GAME_TITLE,
    GAME_PLAYING,
    GAME_WAVE_CLEAR,
    GAME_OVER
};

// ---------------------------------------------------------------------------
// Game: owns everything, runs the frame loop
// ---------------------------------------------------------------------------
class Game {
public:
    Game()
        : mPlayerBullet(0), mScore(0), mHiScore(0), mWave(1),
          mBombCooldown(60), mWaveClearTimer(0), mLastExtraLifeAt(0),
          mState(GAME_TITLE), mDifficulty(DIFF_MEDIUM) {
        // Class-typed members are held BY POINTER: v32c++ never injects
        // constructor calls for class-typed members (its ctor-call
        // injection only walks function bodies), so `Swarm mSwarm;`
        // would leave the grid and vtable as raw stack garbage and
        // HALT on first use. `new T()` runs the constructor and
        // installs the vtable (see the v32_new_* helpers).
        mPlayer = new Player();   // default parameter values fill px/py
        mSwarm  = new Swarm();
        mSaucer = new Saucer();
        mBombs  = new BombList();
        mPlayer->reset(PLAYFIELD_W / 2 - PLAYER_WIDTH / 2, PLAYER_HOME_Y);
        for (int i = 0; i < BUNKER_COUNT; ++i) mBunkers[i] = 0;
        // title screen active: game starts on START (titleFrame).
        // (The old TEMP DEBUG autostart was removed with the headless
        // v32sim testing era -- re-add startNewGame() here for headless
        // runs.)
    }

    ~Game() {
        delete mPlayerBullet;
        for (int i = 0; i < mBombs->size(); ++i) delete (*mBombs)[i];
        for (int i = 0; i < BUNKER_COUNT; ++i) delete mBunkers[i];
        delete mBombs;
        delete mSaucer;
        delete mSwarm;
        delete mPlayer;
    }

    void runFrame(const Input& in) {
        if (mState == GAME_TITLE)           titleFrame(in);
        else if (mState == GAME_PLAYING)    playFrame(in);
        else if (mState == GAME_WAVE_CLEAR) waveClearFrame();
        else                                gameOverFrame(in);
    }

    void draw(Video& video) {
        video.clear();   // hand frame-clearing to the GPU layer
        if (mState == GAME_TITLE) {
            drawText(video, "SPACE INVADERS", 144, 60);
            drawText(video, "PRESS START",   154, 180);
            // difficulty menu: three options, '>' marks the selection
            drawText(video, "EASY",   186, 100);
            drawText(video, "MEDIUM", 186, 120);
            drawText(video, "HARD",   186, 140);
            drawText(video, ">", 176, 100 + mDifficulty * 20);
        } else {
            mPlayer->draw(video);
            mSwarm->draw(video);
            mSaucer->draw(video);
            for (int i = 0; i < BUNKER_COUNT; ++i)
                if (mBunkers[i]) mBunkers[i]->draw(video);
            if (mPlayerBullet) mPlayerBullet->draw(video);
            for (int i = 0; i < mBombs->size(); ++i) (*mBombs)[i]->draw(video);
            drawHUD(video);
        }
    }

private:
    // ---- title ------------------------------------------------------------
    void titleFrame(const Input& in) {
        // a just-pressed button reads exactly 1
        // NOTE: no int round-trip here -- Vircon32 C rejects assigning an
        // int expression to an enum-typed lvalue ("cannot assign int to
        // const-qualified enumeration"), stricter than gcc. Step through
        // the named values instead (also avoids ternaries).
        if (in.read(BTN_UP) == 1 && mDifficulty > DIFF_EASY) {
            if (mDifficulty == DIFF_HARD)     mDifficulty = DIFF_MEDIUM;
            else                             mDifficulty = DIFF_EASY;
        }
        if (in.read(BTN_DOWN) == 1 && mDifficulty < DIFF_HARD) {
            if (mDifficulty == DIFF_EASY)     mDifficulty = DIFF_MEDIUM;
            else                             mDifficulty = DIFF_HARD;
        }
        if (in.read(BTN_START) == 1) startNewGame();
    }

    // bomb spawn cadence by difficulty (frames between drops, before
    // the random jitter): easy breathes, hard is the original pressure
    int bombCooldownBase() const {
        if (mDifficulty == DIFF_EASY)   return 80;
        if (mDifficulty == DIFF_MEDIUM) return 55;
        return 30;
    }

    // bomb fall speed cap by difficulty (px/frame, before wave scaling)
    int bombFallSpeed() const {
        if (mDifficulty == DIFF_EASY)   return 1;
        if (mDifficulty == DIFF_MEDIUM) return 2;
        return 3;
    }

    // enemy weapon strength vs shield hit points, by difficulty:
    // easy chips 1 hp per hit, medium 2 (2 hits per cell), hard 4
    // (one hit destroys a full cell)
    int shieldDamage() const {
        if (mDifficulty == DIFF_EASY)   return 1;
        if (mDifficulty == DIFF_MEDIUM) return 2;
        return 4;
    }

    void startNewGame() {
        mScore = 0;
        mHiScore = 0;
        mWave = 1;
        mWaveClearTimer = 0;
        mLastExtraLifeAt = 0;
        mBombCooldown = bombCooldownBase();
        mSwarm->setDifficulty(mDifficulty);
        mPlayer->reset(PLAYFIELD_W / 2 - PLAYER_WIDTH / 2, PLAYER_HOME_Y);
        for (int i = 0; i < mBombs->size(); ++i) delete (*mBombs)[i];
        mBombs->clear();
        delete mPlayerBullet;
        mPlayerBullet = 0;
        buildWave();
        mState = GAME_PLAYING;
    }

    void buildWave() {
        mSwarm->destroyAll();
        mSwarm->spawn(40 + (mWave - 1) * 20);  // each wave starts lower
        for (int i = 0; i < BUNKER_COUNT; ++i) {
            delete mBunkers[i];
            // spread the four 60px umbrella bunkers across the wide
            // field: 41 + i*102 -> 41, 143, 245, 347 (41px margins,
            // 42px gaps; last ends at 407)
            mBunkers[i] = new Bunker(41 + i * 102, 168);
        }
    }

    // ---- main gameplay ------------------------------------------------------
    void playFrame(const Input& in) {
        mPlayer->handleInput(in);
        if (in.read(BTN_A) == 1 || in.read(BTN_B) == 1) mPlayer->fire();

        updatePlayerBullet();
        mSwarm->update(mSfx);
        updateBombs();
        mSaucer->update();
        mPlayer->update();
        checkCollisions();
        awardExtraLife();

        if (mPlayer->gameOver()) mState = GAME_OVER;
        else if (mSwarm->aliveCount() == 0) mState = GAME_WAVE_CLEAR;
        else if (mSwarm->reachedBottom(PLAYER_HOME_Y)) mState = GAME_OVER;
    }

    void updatePlayerBullet() {
        if (mPlayer->wantsFire() && mPlayerBullet == 0) {
            mPlayerBullet = new Bullet(mPlayer->muzzleX(), mPlayer->muzzleY(),
                                       0, -8, AssetIds::PLAYER_BULLET);
            mSfx.play(AssetIds::SOUND_SHOOT);
        }
        mPlayer->clearFire();
        if (mPlayerBullet) {
            mPlayerBullet->update();
            if (mPlayerBullet->state() == STATE_DEAD) {
                delete mPlayerBullet;
                mPlayerBullet = 0;
            }
        }
    }

    void updateBombs() {
        --mBombCooldown;
        if (mBombCooldown <= 0 && !mBombs->full()) {
            Alien* shooter = mSwarm->randomShooter();
            if (shooter) {
                int sprite;
                if (g_rng.bit()) sprite = AssetIds::ALIEN_BULLET_SQUIGGLE;
                else             sprite = AssetIds::ALIEN_BULLET_PLUMB;
                // wave-scaled fall speed, capped by difficulty
                int vy = 1 + mWave / 3;
                int cap = bombFallSpeed();
                if (vy > cap) vy = cap;
                mBombs->push(new Bullet(
                    shooter->posX() + ALIEN_WIDTH / 2 - SPRITE_W / 2,
                    shooter->posY() + ALIEN_HEIGHT,
                    0, vy, sprite));
            }
            // jitter window also scales down on easier settings, so easy
            // isn't just slower on average but also less spiky
            int jitter = 45;
            if (mDifficulty == DIFF_EASY)   jitter = 70;
            if (mDifficulty == DIFF_MEDIUM) jitter = 55;
            mBombCooldown = bombCooldownBase() + g_rng.next(jitter);
        }
        for (int i = 0; i < mBombs->size();) {
            // KEPT UNHOISTED ON PURPOSE: the result of operator[] (a call)
            // used directly as a virtual-call receiver. This was the
            // trigger of the wild-jump HLT (~frame 160, first bomb
            // delete). v32c++ now auto-hoists these in its lowering
            // (phase 3c, the Vircon32 C arg-staging workaround), so this
            // loop doubles as a live regression test for that fix.
            (*mBombs)[i]->update();
            if ((*mBombs)[i]->state() == STATE_DEAD) {
                delete (*mBombs)[i];
                mBombs->eraseAt(i);
            } else {
                ++i;
            }
        }
    }

    // ---- collisions ---------------------------------------------------------
    void checkCollisions() {
        // player bullet vs aliens
        if (mPlayerBullet) {
            Rect pb;
            mPlayerBullet->getBounds(pb);   // virtual: Bullet's inset box
            Alien* a = mSwarm->hitTest(pb);
            if (a) {
                a->destroy(mSfx);
                addScore(a->points());
                deleteBullet();
            } else if (mSaucer->flying() && mSaucer->collidesWith(pb)) {
                addScore(mSaucer->scoreValue());
                mSaucer->destroy(mSfx);
                deleteBullet();
            } else {
                // vs bunkers
                for (int i = 0; i < BUNKER_COUNT; ++i)
                    if (mBunkers[i] && mBunkers[i]->erode(pb, mSfx, 1)) {
                        deleteBullet();
                        break;
                    }
            }
        }
        // bombs vs player / bunkers (KEPT UNHOISTED on purpose -- same
        // live-regression reasoning as updateBombs above)
        for (int i = 0; i < mBombs->size();) {
            bool gone = false;
            if (mPlayer->collidesWith(*(*mBombs)[i])) {
                mPlayer->hit(mSfx);
                gone = true;
            } else {
                for (int k = 0; k < BUNKER_COUNT && !gone; ++k) {
                    if (mBunkers[k]) {
                        Rect bb;
                        (*mBombs)[i]->getBounds(bb);   // virtual: inset box
                        if (mBunkers[k]->erode(bb, mSfx, shieldDamage()))
                            gone = true;
                    }
                }
            }
            if (gone) { delete (*mBombs)[i]; mBombs->eraseAt(i); }
            else      { ++i; }
        }
    }

    void deleteBullet() {
        delete mPlayerBullet;
        mPlayerBullet = 0;
    }

    // ---- progression --------------------------------------------------------
    void waveClearFrame() {
        ++mWaveClearTimer;
        if (mWaveClearTimer > 120) {
            mWaveClearTimer = 0;
            ++mWave;
            buildWave();
            mState = GAME_PLAYING;
        }
    }

    void gameOverFrame(const Input& in) {
        if (in.read(BTN_START) == 1) mState = GAME_TITLE;
    }

    void addScore(int pts) {
        mScore += pts;
        if (mScore > mHiScore) mHiScore = mScore;
    }

    void awardExtraLife() {
        if (mScore / 1500 > mLastExtraLifeAt / 1500) {
            mPlayer->awardLife();
            mSfx.play(AssetIds::SOUND_EXTRA_LIFE);
        }
        mLastExtraLifeAt = mScore;
    }

    void drawHUD(Video& video) {
        drawNumber(video, mScore,   8,   2);
        drawNumber(video, mHiScore, 88,  2);
        drawNumber(video, mWave,    200, 2);
        video.tint(0xFF00FF00);   // lives icons match the green cannon
        for (int i = 0; i < mPlayer->lives() - 1; ++i)
            video.blit2(AssetIds::PLAYER_SHIP_TURRET,   // '^' over '_'
                        AssetIds::PLAYER_SHIP_BASE,
                        8 + i * 16, 236, PLAYER_TURRET_DROP);
    }

    Player*   mPlayer;      // by pointer: ctor injection never touches
    Swarm*    mSwarm;       //   class-typed members -- see constructor
    Saucer*   mSaucer;
    Bullet*   mPlayerBullet;
    BombList* mBombs;   // concrete fixed-capacity list, no templates
    Bunker*   mBunkers[4];   // BUNKER_COUNT: dims must be INT_LITERALs
    Sound     mSfx;
    int       mScore;
    int       mHiScore;
    int       mWave;
    int       mBombCooldown;
    int       mWaveClearTimer;
    int       mLastExtraLifeAt;
    GameState mState;
    GameDifficulty mDifficulty;   // chosen on the title screen
};

} // namespace si
// === END FILE: si_game.h ===


// === FILE: si_main.cpp ===  (the central file)
// If you split the sections above into real headers, include them here:
//   #include "si_assets.h"
//   #include "si_core.h"
//   #include "si_platform.h"
//   #include "si_entities.h"
//   #include "si_game.h"

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
// === END FILE: si_main.cpp ===
