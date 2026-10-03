#pragma once
// *****************************************************************************
//  entities.hpp — everything that moves or gets shot
//
//      Entity                  position, size, life cycle, collision
//       +- Player              the cannon
//       +- Bullet              the player's shot and the aliens' bombs
//       +- Alien               one invader
//       |   +- AlienTopRow / AlienMiddleRow / AlienBottomRow
//       +- Bunker              a destructible shield
//       +- Saucer              the mystery ship
// *****************************************************************************
#include "core.hpp"
#include "platform.hpp"

namespace si {

// ---------------------------------------------------------------------------
// Entity: the base of the hierarchy. update() and draw() are pure virtual;
// bounds() has a default (the full cell) that thin-glyph classes override.
// ---------------------------------------------------------------------------
class Entity {
public:
    Entity(int px, int py, int w, int h) : mPos(px, py), mW(w), mH(h) {}
    virtual ~Entity() {}

    virtual void update() = 0;              // one frame of simulation
    virtual void draw(Video& video) = 0;

    int posX() const { return mPos.x; }
    int posY() const { return mPos.y; }
    void move(int dx, int dy) { mPos += Vec2(dx, dy); }

    // The collision box. mPos is always the CELL position (what drawing,
    // clamping and muzzle math use); a subclass whose glyph is much
    // smaller than its cell returns an inset box instead.
    virtual Rect bounds() const { return Rect(mPos.x, mPos.y, mW, mH); }

    EntityState state() const { return mState; }
    bool alive() const        { return mState == STATE_ALIVE; }
    void kill()               { mState = STATE_DEAD; }

    bool collidesWith(const Rect& r) const {
        Rect mine = bounds();
        return mine.intersects(r);
    }
    bool collidesWith(const Entity& other) const {
        Rect theirs = other.bounds();
        return collidesWith(theirs);
    }

protected:
    Vec2        mPos;
    int         mW, mH;
    EntityState mState = STATE_ALIVE;
    int         mTimer = 0;          // frames left in STATE_DYING

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
    Player(int px = 0, int py = PLAYER_HOME_Y)
        : Entity(px, py, PLAYER_WIDTH, PLAYER_HEIGHT) {}

    // back to a fresh three-life cannon at (px, py)
    void reset(int px, int py) {
        mPos = Vec2(px, py);
        mLives = 3;
        mRespawn = 0;
        mWantsFire = false;
        mState = STATE_ALIVE;
        mTimer = 0;
    }

    // the visible turret+base band is roughly the lower 8 px of the cell
    Rect bounds() const {
        return Rect(mPos.x, mPos.y + PLAYER_HIT_INSET_Y,
                    PLAYER_WIDTH, PLAYER_HEIGHT - 2 * PLAYER_HIT_INSET_Y);
    }

    void handleInput(const Input& in) {
        if (!alive()) return;
        if (in.held(BTN_LEFT))  mPos.x -= PLAYER_SPEED;
        if (in.held(BTN_RIGHT)) mPos.x += PLAYER_SPEED;
        if (mPos.x < 0) mPos.x = 0;
        if (mPos.x > PLAYFIELD_W - PLAYER_WIDTH) mPos.x = PLAYFIELD_W - PLAYER_WIDTH;
        if (in.pressed(BTN_A) || in.pressed(BTN_B)) mWantsFire = true;
    }

    void update() {
        if (mState == STATE_DYING) {
            tickDeath();
            if (mState == STATE_DEAD) mRespawn = PLAYER_RESPAWN_FRAMES;
        } else if (mState == STATE_DEAD && mLives > 0) {
            --mRespawn;
            if (mRespawn == 0) {
                mPos.y = PLAYER_HOME_Y;
                mState = STATE_ALIVE;
            }
        }
    }

    void draw(Video& video) {
        if (mState == STATE_ALIVE) {
            video.tint(color_green);
            video.blit2(AssetIds::PLAYER_SHIP_TURRET, AssetIds::PLAYER_SHIP_BASE,
                        mPos.x, mPos.y, PLAYER_TURRET_DROP);
        } else if (mState == STATE_DYING) {
            video.tint(color_orange);
            video.blit(AssetIds::PLAYER_EXPLOSION, mPos.x, mPos.y);
        }
    }

    bool wantsFire() const { return mWantsFire; }
    void clearFire()       { mWantsFire = false; }

    // where a new shot starts: one cell above the cannon
    Vec2 muzzle() const { return Vec2(mPos.x + PLAYER_WIDTH / 2, mPos.y - SPRITE_H); }

    void hit(Sound& sfx) {
        if (!alive()) return;
        --mLives;
        startDying(PLAYER_DEATH_FRAMES);
        sfx.play(AssetIds::SOUND_PLAYER_DEATH);
    }

    int  lives() const    { return mLives; }
    void awardLife()      { ++mLives; }
    bool gameOver() const { return mLives <= 0 && mState == STATE_DEAD; }

private:
    int  mLives = 3;
    int  mRespawn = 0;
    bool mWantsFire = false;
};

// ---------------------------------------------------------------------------
// Bullet: the player's shot and the aliens' bombs. A plain value -- the
// bombs live directly in a std::vector<Bullet>.
// ---------------------------------------------------------------------------
class Bullet : public Entity {
public:
    Bullet(const Vec2& at, const Vec2& velocity, int spriteId)
        : Entity(at.x, at.y, SPRITE_W, SPRITE_H), mVel(velocity.x, velocity.y),
          mSprite(spriteId) {}

    // '|' is a ~2 px stroke and 'v' a small chevron inside the 10x20 cell
    Rect bounds() const {
        return Rect(mPos.x + BULLET_HIT_INSET_X, mPos.y + BULLET_HIT_INSET_Y,
                    SPRITE_W - 2 * BULLET_HIT_INSET_X, SPRITE_H - 2 * BULLET_HIT_INSET_Y);
    }

    void update() {
        mPos += mVel;
        if (mPos.y < -SPRITE_H || mPos.y > PLAYFIELD_H) mState = STATE_DEAD;
    }

    void draw(Video& video) {
        video.tint(color_white);
        video.blit(mSprite, mPos.x, mPos.y);
    }

private:
    Vec2 mVel;
    int  mSprite;
};

// ---------------------------------------------------------------------------
// Invaders: Alien, plus one subclass per kind (they differ in their two
// animation frames and their score).
// ---------------------------------------------------------------------------
class Alien : public Entity {
public:
    Alien(int px, int py, int points, int color)
        : Entity(px, py, ALIEN_WIDTH, ALIEN_HEIGHT), mPoints(points), mColor(color) {}

    void update() { tickDeath(); }    // DYING -> DEAD once the poof is over

    void draw(Video& video) {
        bool dying = (mState == STATE_DYING);
        video.tint(dying ? color_orange : mColor);
        video.blit(dying ? AssetIds::ALIEN_EXPLOSION : spriteForFrame(mFrame),
                   mPos.x, mPos.y);
    }

    int  points() const   { return mPoints; }
    void setFrame(int f)  { mFrame = f; }     // the Swarm drives the march

    void destroy(Sound& sfx) {
        if (!alive()) return;
        startDying(ALIEN_EXPLOSION_FRAMES);
        sfx.play(AssetIds::SOUND_ALIEN_DEATH);
    }

protected:
    virtual int spriteForFrame(int frame) const = 0;

    int mPoints;
    int mColor;        // this alien's row colour (ABGR)
    int mFrame = 0;
};

class AlienTopRow : public Alien {           // squid, 30 points
public:
    AlienTopRow(int px, int py, int color) : Alien(px, py, 30, color) {}
protected:
    int spriteForFrame(int f) const {
        return (f == 0) ? AssetIds::ALIEN_A_FRAME0 : AssetIds::ALIEN_A_FRAME1;
    }
};

class AlienMiddleRow : public Alien {        // crab, 20 points
public:
    AlienMiddleRow(int px, int py, int color) : Alien(px, py, 20, color) {}
protected:
    int spriteForFrame(int f) const {
        return (f == 0) ? AssetIds::ALIEN_B_FRAME0 : AssetIds::ALIEN_B_FRAME1;
    }
};

class AlienBottomRow : public Alien {        // octopus, 10 points
public:
    AlienBottomRow(int px, int py, int color) : Alien(px, py, 10, color) {}
protected:
    int spriteForFrame(int f) const {
        return (f == 0) ? AssetIds::ALIEN_C_FRAME0 : AssetIds::ALIEN_C_FRAME1;
    }
};

// ---------------------------------------------------------------------------
// Bunker: a small grid of cells, each with a few hit points. The four
// bunkers are values in a std::array; rebuild() puts one back at full
// strength for a new wave.
//
//     row 0:  . X X X X .     chamfered top corners
//     row 1:  X X X X X X     solid middle
//     row 2:  X X . . X X     legs, with an arch underneath
// ---------------------------------------------------------------------------
class Bunker : public Entity {
public:
    Bunker() : Entity(0, BUNKER_Y, BUNKER_CELLS_W * BUNKER_CELL_W,
                                   BUNKER_CELLS_H * BUNKER_CELL_H) {
        rebuild(0);
    }

    void rebuild(int px) {
        mPos.x = px;
        for (int cy = 0; cy < BUNKER_CELLS_H; ++cy)
            for (int cx = 0; cx < BUNKER_CELLS_W; ++cx)
                mCells[cy][cx] = shapeHas(cx, cy) ? BUNKER_CELL_HP : 0;
    }

    void update() {}

    void draw(Video& video) {
        video.tint(color_green);
        for (int cy = 0; cy < BUNKER_CELLS_H; ++cy)
            for (int cx = 0; cx < BUNKER_CELLS_W; ++cx)
                if (mCells[cy][cx] > 0)
                    // hit points 1..4 -> blocks 0x11..0x14 (lightest..solid)
                    video.blit(AssetIds::BUNKER_BLOCK_1 + mCells[cy][cx] - 1,
                               mPos.x + cx * BUNKER_CELL_W, mPos.y + cy * BUNKER_CELL_H);
    }

    // Every cell the rectangle overlaps loses `damage` hit points.
    // Returns true if anything was chipped.
    bool erode(const Rect& hit, Sound& sfx, int damage) {
        bool any = false;
        for (int cy = 0; cy < BUNKER_CELLS_H; ++cy) {
            for (int cx = 0; cx < BUNKER_CELLS_W; ++cx) {
                if (mCells[cy][cx] <= 0) continue;
                Rect cell(mPos.x + cx * BUNKER_CELL_W, mPos.y + cy * BUNKER_CELL_H,
                          BUNKER_CELL_W, BUNKER_CELL_H);
                if (!cell.intersects(hit)) continue;
                mCells[cy][cx] -= damage;
                if (mCells[cy][cx] < 0) mCells[cy][cx] = 0;
                any = true;
            }
        }
        if (any) sfx.play(AssetIds::SOUND_BUNKER_HIT);
        return any;
    }

private:
    // is (cx, cy) inside the umbrella outline?
    bool shapeHas(int cx, int cy) const {
        if (cy == 0) return cx > 0 && cx < BUNKER_CELLS_W - 1;
        if (cy == BUNKER_CELLS_H - 1) return cx < 2 || cx >= BUNKER_CELLS_W - 2;
        return true;
    }

    int mCells[BUNKER_CELLS_H][BUNKER_CELLS_W];   // hit points, 0 = gone
};

// ---------------------------------------------------------------------------
// Saucer: the mystery ship that crosses the top of the screen.
//
// Built from three glyphs of the BIOS font instead of one:
//
//        ___              a lowercase 'o', stretched wide: the dome
//      _(___)_
//     (_|__|__)           a capital 'O', stretched wider: the hull,
//                         with '|' rim lights sliding across it
//
// The lights sit on a circle seen edge-on. Each has an angle; its x is the
// sine of that angle and it is only drawn while its cosine is positive
// (the half of the circle facing us), brightest at the middle. Advancing
// every angle a little each few frames makes them slide across the hull
// and wrap round the back -- the saucer appears to spin.
// ---------------------------------------------------------------------------
class Saucer : public Entity {
public:
    // kill() because Entity starts ALIVE and the saucer starts in hiding
    Saucer() : Entity(0, SAUCER_Y, SAUCER_WIDTH, SAUCER_HEIGHT) { kill(); }

    void update() {
        ++mSpin;
        if (mState == STATE_ALIVE) {
            mPos.x += mDir * SAUCER_SPEED;
            if (mPos.x < -SAUCER_WIDTH || mPos.x > PLAYFIELD_W) {
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
        if (mState == STATE_DEAD) return;
        if (mState == STATE_DYING) {
            video.tint(color_orange);
            video.blitScaled(AssetIds::ALIEN_EXPLOSION, mPos.x, mPos.y - 2, 4.0, 1.0);
            return;
        }
        video.tint(color_red);
        // Stretching a glyph stretches its whole 10 x 20 cell, blank
        // margins included: at 0.75 high the 'O' fills rows 2..11 of its
        // cell and the 'o' rows 5..11, hence the two different offsets
        // that make the dome's base overlap the hull's top edge.
        video.blitScaled(AssetIds::SAUCER_DOME, mPos.x + 10, mPos.y - 4, 2.0, 0.75);
        video.blitScaled(AssetIds::SAUCER_HULL, mPos.x,      mPos.y + 3, 4.0, 0.75);
        drawLights(video);
    }

    void launch() {
        mDir = g_rng.coin() ? 1 : -1;
        mPos.x = (mDir > 0) ? -SAUCER_WIDTH : PLAYFIELD_W;
        mState = STATE_ALIVE;
    }

    int scoreValue() const { return 50 + 10 * g_rng.next(8); }   // 50..120

    void destroy(Sound& sfx) {
        if (!alive()) return;
        startDying(ALIEN_EXPLOSION_FRAMES);
        sfx.play(AssetIds::SOUND_SAUCER_DEATH);
    }

    bool flying() const { return alive(); }

private:
    void drawLights(Video& video) {
        // sixteenths of a turn; the lights spin against the flight direction
        int turn = (mSpin / SAUCER_SPIN_FRAMES) * mDir;
        for (int i = 0; i < SAUCER_LIGHTS; ++i) {
            int angle  = (turn + i * 16 / SAUCER_LIGHTS) & 15;
            int facing = SINE16[(angle + 4) & 15];      // cosine: > 0 = our side
            if (facing <= 0) continue;
            // across the hull: the sine sweeps -16..16, the rim is 14 px
            // either side of the middle
            int x = mPos.x + SAUCER_WIDTH / 2 + SINE16[angle] * 14 / 16;
            video.tint(facing > 12 ? color_white : color_yellow);
            video.blitScaled(AssetIds::SAUCER_LIGHT, x - 5, mPos.y + 6, 1.0, 0.4);
        }
    }

    int mDir = 1;
    int mCooldown = 600;
    int mSpin = 0;
};

} // namespace si
