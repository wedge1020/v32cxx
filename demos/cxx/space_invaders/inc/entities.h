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
