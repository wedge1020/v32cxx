#include "video.h"
#include "audio.h"
#include "input.h"

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
//  the 16-bit range (playfield is 224 x 256).
//
//  Platform hookup notes:
//    * All drawing is stubbed: every class has draw()  -- fill in with your
//      blitter calls using the integer sprite ids listed below.
//    * All audio is stubbed: sound effects are triggered via Sound::play().
//    * All input is stubbed: Input::read(Button) returns one signed int per
//      button, never zero.
//    * Video::sync() (NOT part of Input) signals end-of-frame to the GPU:
//      vsync / frame flip / present. Call it once per frame after drawing.
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

// === FILE: si_assets.h ===
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
// === END FILE: si_assets.h ===


// === FILE: si_core.h ===  (freestanding replacements for library facilities)
namespace si {

// ---------------------------------------------------------------------------
// Random: small deterministic LCG, replaces <cstdlib> rand().
// Plain 'int' state (no 'unsigned' in the subset).
// ---------------------------------------------------------------------------
class Random {
public:
    Random() : mState(0x1234ABCD) {}

    void seed(int s) { mState = s; }

    // returns 0..limit-1
    int next(int limit) {
        mState = mState * 1103515245 + 12345;
        return ((mState >> 16) & 0x7FFF) % limit;
    }

    // returns 0 or 1
    int bit() { return next(2); }

private:
    int mState;
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

    int x() const { return mV >> 16; }
    int y() const { return (mV << 16) >> 16; }   // sign-extend low half

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


// === FILE: si_platform.h ===  (the three platform stub layers)
namespace si {

// ---------------------------------------------------------------------------
// Input: per-button signed reads, never zero. NO sync() here.
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
        // TODO: platform I/O -- poll just this button and return its
        //       signed frame count. Placeholder returns "never touched".
        return -1;
    }
};

// ---------------------------------------------------------------------------
// Video: GPU-facing layer. Owns end-of-frame sync (vsync / flip / present),
// sprite blitting, and clearing -- NOT part of Input.
// ---------------------------------------------------------------------------
class Video {
public:
    // Blit one sprite (10x20) with the given ASCII-value sprite id.
    void blit(int spriteId, int x, int y) {
        // TODO: platform GPU call -- draw sprite `spriteId` at (x, y)
    }

    // Clear the framebuffer / reset the draw list.
    void clear() {
        // TODO: platform GPU call
    }

    // Signal end of processing for this frame: flip buffers, wait for
    // vblank, present. Call once per frame after all drawing is done.
    void sync() {
        // TODO: platform GPU call -- vsync / present / frame flip
    }
};

// ---------------------------------------------------------------------------
// Sound: stubbed playback of integer sound ids
// ---------------------------------------------------------------------------
class Sound {
public:
    void play(int soundId) {
        // TODO: start playing the given sound
    }
    void stop(int soundId) {
        // TODO
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
    // not be returned by value under Vircon32 C's one-word rule
    void getBounds(Rect& r) const {
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
    // NOTE: default ctor exists because Game cannot mention mPlayer in its
    // initializer list (sema rejects member inits for class-typed fields);
    // Game default-constructs it and immediately calls reset(px, py).
    Player()
        : Entity(0, PLAYER_HOME_Y, PLAYER_WIDTH, PLAYER_HEIGHT),
          mLives(3), mRespawn(0), mWantsFire(false) {}

    Player(int px, int py)
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
        if (mState == STATE_ALIVE)
            video.blit(AssetIds::PLAYER_SHIP, mPos.x(), mPos.y());      // '='
        else if (mState == STATE_DYING)
            video.blit(AssetIds::PLAYER_EXPLOSION, mPos.x(), mPos.y()); // '#'
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

    void update() {
        mPos += mVel;   // Vec2::operator+=
        if (mPos.y() < -SPRITE_H || mPos.y() > PLAYFIELD_H)
            mState = STATE_DEAD;
    }

    void draw(Video& video) {
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
          mPoints(points), mFrame(0) {}

    void update() {
        tickDeath(); // DYING -> DEAD after explosion frames
    }

    void draw(Video& video) {
        video.blit(spriteId(), mPos.x(), mPos.y());   // 10x20, ASCII id
    }

    int points() const { return mPoints; }
    int frame() const  { return mFrame; }

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
        for (int i = 0; i < BUNKER_CELLS_W * BUNKER_CELLS_H; ++i)
            mCells[i] = true;
    }

    void update() {}

    void draw(Video& video) {
        for (int cy = 0; cy < BUNKER_CELLS_H; ++cy)
            for (int cx = 0; cx < BUNKER_CELLS_W; ++cx)
                if (cell(cx, cy))
                    video.blit(AssetIds::BUNKER_BLOCK,          // '@'
                               mPos.x() + cx * BUNKER_CELL_W,
                               mPos.y() + cy * BUNKER_CELL_H);
    }

    // erode cells where the rect overlaps; returns true if anything erased
    bool erode(const Rect& hit, Sound& sfx) {
        bool any = false;
        for (int cy = 0; cy < BUNKER_CELLS_H; ++cy) {
            for (int cx = 0; cx < BUNKER_CELLS_W; ++cx) {
                if (!cell(cx, cy)) continue;
                Rect r;
                r.x = mPos.x() + cx * BUNKER_CELL_W;
                r.y = mPos.y() + cy * BUNKER_CELL_H;
                r.w = BUNKER_CELL_W;
                r.h = BUNKER_CELL_H;
                if (r.intersects(hit)) {
                    setCell(cx, cy, false);
                    any = true;
                }
            }
        }
        if (any) sfx.play(AssetIds::SOUND_BUNKER_HIT);
        return any;
    }

private:
    bool cell(int x, int y) const {
        return mCells[y * BUNKER_CELLS_W + x];
    }
    void setCell(int x, int y, bool v) {
        mCells[y * BUNKER_CELLS_W + x] = v;
    }
    bool mCells[6];      // BUNKER_CELLS_W * BUNKER_CELLS_H: dims must be
                         // INT_LITERALs, not enum constants or expressions
};

// ---------------------------------------------------------------------------
// Mystery saucer
// ---------------------------------------------------------------------------
class Saucer : public Entity {
public:
    Saucer() : Entity(0, 16, SAUCER_WIDTH, SAUCER_HEIGHT),
               mDir(1), mCooldown(600) {}

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
        if (mState != STATE_DEAD)
            video.blit(AssetIds::SAUCER, mPos.x(), mPos.y());   // 'U'
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
    Swarm() : mDx(2), mAnimFrame(0), mStepCooldown(0) {
        mOffset.setX(0);
        mOffset.setY(0);
        for (int r = 0; r < SWARM_ROWS; ++r)
            for (int c = 0; c < SWARM_COLS; ++c)
                mGrid[r][c] = 0;
    }

    void spawn(int baseY) {
        for (int r = 0; r < SWARM_ROWS; ++r) {
            for (int c = 0; c < SWARM_COLS; ++c) {
                int px = c * SWARM_GAP_X;
                int py = baseY + r * SWARM_GAP_Y;
                Alien* a;
                if (r == 0)      a = new AlienTopRow(px, py);
                else if (r < 3)  a = new AlienMiddleRow(px, py);
                else             a = new AlienBottomRow(px, py);
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

    // march one animation/exchange step; steps faster as aliens die
    void step(Sound& sfx) {
        if (mStepCooldown > 0) { --mStepCooldown; return; }
        mStepCooldown = stepInterval();

        // find horizontal extent of living columns
        int minC = SWARM_COLS, maxC = -1;
        for (int r = 0; r < SWARM_ROWS; ++r)
            for (int c = 0; c < SWARM_COLS; ++c)
                if (mGrid[r][c] && mGrid[r][c]->state() == STATE_ALIVE) {
                    if (c < minC) minC = c;
                    if (c > maxC) maxC = c;
                }
        if (maxC < 0) return; // nobody left

        int leftEdge  = minC * SWARM_GAP_X + mOffset.x();
        int rightEdge = (maxC + 1) * SWARM_GAP_X + mOffset.x();

        bool drop = (mDx > 0 && rightEdge + mDx > PLAYFIELD_W) ||
                    (mDx < 0 && leftEdge  + mDx < 0);
        if (drop) {
            mDx = -mDx;
            mOffset.setY(mOffset.y() + SPRITE_H);   // one sprite height
        } else {
            mOffset.setX(mOffset.x() + mDx);
        }

        mAnimFrame = 1 - mAnimFrame;
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

    // pick a random living alien (lowest in its column) to drop a bomb from
    Alien* randomShooter() {
        int alive = aliveCount();
        if (!alive) return 0;
        int pick = g_rng.next(alive);
        for (int c = 0; c < SWARM_COLS; ++c) {
            for (int r = SWARM_ROWS - 1; r >= 0; --r) {
                Alien* a = mGrid[r][c];
                if (a && a->state() == STATE_ALIVE) {
                    if (pick == 0) return a;
                    --pick;
                    break; // only the lowest alien per column shoots
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
        if (n > 40) return 30;
        if (n > 30) return 24;
        if (n > 20) return 18;
        if (n > 10) return 12;
        if (n >  5) return 8;
        if (n >  1) return 4;
        return 2;
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
    Vec2   mOffset;
    int    mDx;
    int    mAnimFrame;
    int    mStepCooldown;

    friend class Game;   // Game may reach into the grid directly
};

// ---------------------------------------------------------------------------
// HUD text helpers (free functions: the subset has no static member
// functions, and 'ScoreDisplay::draw' would need them)
// ---------------------------------------------------------------------------
void drawNumber(Video& video, int value, int x, int y) {
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
          mState(GAME_TITLE) {
        // mPlayer is class-typed: default-constructed, then positioned here
        mPlayer.reset(PLAYFIELD_W / 2 - PLAYER_WIDTH / 2, PLAYER_HOME_Y);
        for (int i = 0; i < BUNKER_COUNT; ++i) mBunkers[i] = 0;
    }

    ~Game() {
        delete mPlayerBullet;
        for (int i = 0; i < mBombs.size(); ++i) delete mBombs[i];
        for (int i = 0; i < BUNKER_COUNT; ++i) delete mBunkers[i];
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
            drawText(video, "SPACE INVADERS", 40, 80);
            drawText(video, "PRESS START",   60, 130);
        } else {
            mPlayer.draw(video);
            mSwarm.draw(video);
            mSaucer.draw(video);
            for (int i = 0; i < BUNKER_COUNT; ++i)
                if (mBunkers[i]) mBunkers[i]->draw(video);
            if (mPlayerBullet) mPlayerBullet->draw(video);
            for (int i = 0; i < mBombs.size(); ++i) mBombs[i]->draw(video);
            drawHUD(video);
        }
    }

private:
    // ---- title ------------------------------------------------------------
    void titleFrame(const Input& in) {
        // a just-pressed button reads exactly 1
        if (in.read(BTN_START) == 1) startNewGame();
    }

    void startNewGame() {
        mScore = 0;
        mHiScore = 0;
        mWave = 1;
        mWaveClearTimer = 0;
        mLastExtraLifeAt = 0;
        mBombCooldown = 60;
        mPlayer.reset(PLAYFIELD_W / 2 - PLAYER_WIDTH / 2, PLAYER_HOME_Y);
        for (int i = 0; i < mBombs.size(); ++i) delete mBombs[i];
        mBombs.clear();
        delete mPlayerBullet;
        mPlayerBullet = 0;
        buildWave();
        mState = GAME_PLAYING;
    }

    void buildWave() {
        mSwarm.destroyAll();
        mSwarm.spawn(40 + (mWave - 1) * 20);  // each wave starts lower
        for (int i = 0; i < BUNKER_COUNT; ++i) {
            delete mBunkers[i];
            mBunkers[i] = new Bunker(22 + i * 56, 168);
        }
    }

    // ---- main gameplay ------------------------------------------------------
    void playFrame(const Input& in) {
        mPlayer.handleInput(in);
        if (in.read(BTN_A) == 1 || in.read(BTN_B) == 1) mPlayer.fire();

        updatePlayerBullet();
        mSwarm.update(mSfx);
        updateBombs();
        mSaucer.update();
        mPlayer.update();
        checkCollisions();
        awardExtraLife();

        if (mPlayer.gameOver()) mState = GAME_OVER;
        else if (mSwarm.aliveCount() == 0) mState = GAME_WAVE_CLEAR;
        else if (mSwarm.reachedBottom(PLAYER_HOME_Y)) mState = GAME_OVER;
    }

    void updatePlayerBullet() {
        if (mPlayer.wantsFire() && mPlayerBullet == 0) {
            mPlayerBullet = new Bullet(mPlayer.muzzleX(), mPlayer.muzzleY(),
                                       0, -8, AssetIds::PLAYER_BULLET);
            mSfx.play(AssetIds::SOUND_SHOOT);
        }
        mPlayer.clearFire();
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
        if (mBombCooldown <= 0 && !mBombs.full()) {
            Alien* shooter = mSwarm.randomShooter();
            if (shooter) {
                int sprite;
                if (g_rng.bit()) sprite = AssetIds::ALIEN_BULLET_SQUIGGLE;
                else             sprite = AssetIds::ALIEN_BULLET_PLUMB;
                mBombs.push(new Bullet(
                    shooter->posX() + ALIEN_WIDTH / 2 - SPRITE_W / 2,
                    shooter->posY() + ALIEN_HEIGHT,
                    0, 1 + mWave / 3, sprite));
            }
            mBombCooldown = 30 + g_rng.next(45);
        }
        for (int i = 0; i < mBombs.size();) {
            mBombs[i]->update();
            if (mBombs[i]->state() == STATE_DEAD) {
                delete mBombs[i];
                mBombs.eraseAt(i);
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
            mPlayerBullet->getBounds(pb);
            Alien* a = mSwarm.hitTest(pb);
            if (a) {
                a->destroy(mSfx);
                addScore(a->points());
                deleteBullet();
            } else if (mSaucer.flying() && mSaucer.collidesWith(pb)) {
                addScore(mSaucer.scoreValue());
                mSaucer.destroy(mSfx);
                deleteBullet();
            } else {
                // vs bunkers
                for (int i = 0; i < BUNKER_COUNT; ++i)
                    if (mBunkers[i] && mBunkers[i]->erode(pb, mSfx)) {
                        deleteBullet();
                        break;
                    }
            }
        }
        // bombs vs player / bunkers
        for (int i = 0; i < mBombs.size();) {
            bool gone = false;
            if (mPlayer.collidesWith(*mBombs[i])) {
                mPlayer.hit(mSfx);
                gone = true;
            } else {
                for (int k = 0; k < BUNKER_COUNT && !gone; ++k) {
                    if (mBunkers[k]) {
                        Rect bb;
                        mBombs[i]->getBounds(bb);
                        if (mBunkers[k]->erode(bb, mSfx)) gone = true;
                    }
                }
            }
            if (gone) { delete mBombs[i]; mBombs.eraseAt(i); }
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
            mPlayer.awardLife();
            mSfx.play(AssetIds::SOUND_EXTRA_LIFE);
        }
        mLastExtraLifeAt = mScore;
    }

    void drawHUD(Video& video) {
        drawNumber(video, mScore,   8,   2);
        drawNumber(video, mHiScore, 88,  2);
        drawNumber(video, mWave,    200, 2);
        for (int i = 0; i < mPlayer.lives() - 1; ++i)
            video.blit(AssetIds::PLAYER_SHIP, 8 + i * 16, 236);   // '='
    }

    Player    mPlayer;
    Swarm     mSwarm;
    Saucer    mSaucer;
    Bullet*   mPlayerBullet;
    BombList  mBombs;   // concrete fixed-capacity list, no templates
    Bunker*   mBunkers[4];   // BUNKER_COUNT: dims must be INT_LITERALs
    Sound     mSfx;
    int       mScore;
    int       mHiScore;
    int       mWave;
    int       mBombCooldown;
    int       mWaveClearTimer;
    int       mLastExtraLifeAt;
    GameState mState;
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

    for (;;) {
        // 1) read controller state (per-button, never 0, signed frame counts)
        //    -- the Game queries buttons via input.read(BTN_x) during update.
        //    TODO: make Input::read() hit real controller I/O.

        // 2) simulate one frame
        game.runFrame(input);

        // 3) draw one frame (entity draw() calls go through Video::blit)
        game.draw(video);

        // 4) end-of-frame GPU sync: vsync / flip / present.
        //    Lives on Video, not Input.
        video.sync();
    }
    return 0;
}
// === END FILE: si_main.cpp ===
