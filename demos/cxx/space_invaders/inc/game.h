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
        // Class-typed members are held BY POINTER: v32c++ never injects
        // constructor calls for class-typed members (its ctor-call
        // injection only walks function bodies), so `Swarm mSwarm;`
        // would leave the grid and vtable as raw stack garbage and
        // HALT on first use. `new T()` runs the constructor and
        // installs the vtable (see the v32_new_* helpers).
        mPlayer = new Player();
        mSwarm  = new Swarm();
        mSaucer = new Saucer();
        mBombs  = new BombList();
        mPlayer->reset(PLAYFIELD_W / 2 - PLAYER_WIDTH / 2, PLAYER_HOME_Y);
        for (int i = 0; i < BUNKER_COUNT; ++i) mBunkers[i] = 0;
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
            drawText(video, "SPACE INVADERS", 40, 80);
            drawText(video, "PRESS START",   60, 130);
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
        if (in.read(BTN_START) == 1) startNewGame();
    }

    void startNewGame() {
        mScore = 0;
        mHiScore = 0;
        mWave = 1;
        mWaveClearTimer = 0;
        mLastExtraLifeAt = 0;
        mBombCooldown = 60;
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
            mBunkers[i] = new Bunker(22 + i * 56, 168);
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
                mBombs->push(new Bullet(
                    shooter->posX() + ALIEN_WIDTH / 2 - SPRITE_W / 2,
                    shooter->posY() + ALIEN_HEIGHT,
                    0, 1 + mWave / 3, sprite));
            }
            mBombCooldown = 30 + g_rng.next(45);
        }
        for (int i = 0; i < mBombs->size();) {
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
            mPlayerBullet->getBounds(pb);
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
                    if (mBunkers[i] && mBunkers[i]->erode(pb, mSfx)) {
                        deleteBullet();
                        break;
                    }
            }
        }
        // bombs vs player / bunkers
        for (int i = 0; i < mBombs->size();) {
            bool gone = false;
            if (mPlayer->collidesWith(*(*mBombs)[i])) {
                mPlayer->hit(mSfx);
                gone = true;
            } else {
                for (int k = 0; k < BUNKER_COUNT && !gone; ++k) {
                    if (mBunkers[k]) {
                        Rect bb;
                        (*mBombs)[i]->getBounds(bb);
                        if (mBunkers[k]->erode(bb, mSfx)) gone = true;
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
        for (int i = 0; i < mPlayer->lives() - 1; ++i)
            video.blit(AssetIds::PLAYER_SHIP, 8 + i * 16, 236);   // '='
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
};

} // namespace si
