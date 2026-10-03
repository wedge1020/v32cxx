#pragma once
// *****************************************************************************
//  swarm.hpp — the 5 x 11 formation and its march
// *****************************************************************************
#include "entities.hpp"

namespace si {

// Chosen on the title screen. Slows the march and the bombing.
enum GameDifficulty {
    DIFF_EASY,
    DIFF_MEDIUM,
    DIFF_HARD      // the original tuning
};

// The Atari-rainbow look: one colour per row, top to bottom.
const int ROW_COLORS[SWARM_ROWS] = {
    color_magenta,   // squids
    color_orange,    // crabs
    color_yellow,
    color_green,     // octopuses
    color_cyan
};

class Swarm {
public:
    Swarm() {
        for (int r = 0; r < SWARM_ROWS; ++r)
            for (int c = 0; c < SWARM_COLS; ++c)
                mGrid[r][c] = nullptr;
    }
    ~Swarm() { destroyAll(); }

    void setDifficulty(GameDifficulty d) { mDifficulty = d; }

    // A fresh formation with its top row at baseY, centered on the field.
    void spawn(int baseY) {
        destroyAll();
        int x0 = (PLAYFIELD_W - SWARM_COLS * SWARM_GAP_X) / 2;
        for (int r = 0; r < SWARM_ROWS; ++r) {
            for (int c = 0; c < SWARM_COLS; ++c) {
                int px = x0 + c * SWARM_GAP_X;
                int py = baseY + r * SWARM_GAP_Y;
                Alien* a;
                if (r == 0)      a = new AlienTopRow(px, py, ROW_COLORS[r]);
                else if (r < 3)  a = new AlienMiddleRow(px, py, ROW_COLORS[r]);
                else             a = new AlienBottomRow(px, py, ROW_COLORS[r]);
                mGrid[r][c] = a;
            }
        }
    }

    void destroyAll() {
        for (int r = 0; r < SWARM_ROWS; ++r)
            for (int c = 0; c < SWARM_COLS; ++c) {
                delete mGrid[r][c];
                mGrid[r][c] = nullptr;
            }
    }

    // aliens not yet fully gone (alive or mid-explosion)
    int aliveCount() const {
        int n = 0;
        for (int r = 0; r < SWARM_ROWS; ++r)
            for (int c = 0; c < SWARM_COLS; ++c)
                if (mGrid[r][c] && mGrid[r][c]->state() != STATE_DEAD) ++n;
        return n;
    }

    bool reachedBottom(int limitY) const {
        for (int r = SWARM_ROWS - 1; r >= 0; --r)
            for (int c = 0; c < SWARM_COLS; ++c) {
                Alien* a = mGrid[r][c];
                if (a && a->alive() && a->posY() + ALIEN_HEIGHT >= limitY) return true;
            }
        return false;
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

    // A random alien to drop a bomb: only the lowest living alien of each
    // column can shoot. Counts the candidates, then picks among them.
    Alien* randomShooter() {
        int shooters = 0;
        for (int c = 0; c < SWARM_COLS; ++c)
            if (lowestInColumn(c)) ++shooters;
        if (shooters == 0) return nullptr;
        int pick = g_rng.next(shooters);
        for (int c = 0; c < SWARM_COLS; ++c) {
            Alien* a = lowestInColumn(c);
            if (!a) continue;
            if (pick == 0) return a;
            --pick;
        }
        return nullptr;
    }

    // the living alien this rectangle touches, if any
    Alien* hitTest(const Rect& shot) {
        for (int r = 0; r < SWARM_ROWS; ++r)
            for (int c = 0; c < SWARM_COLS; ++c) {
                Alien* a = mGrid[r][c];
                if (a && a->alive() && a->collidesWith(shot)) return a;
            }
        return nullptr;
    }

private:
    Alien* lowestInColumn(int c) {
        for (int r = SWARM_ROWS - 1; r >= 0; --r) {
            Alien* a = mGrid[r][c];
            if (a && a->alive()) return a;
        }
        return nullptr;
    }

    // One march step: every living alien shifts by mDx, or the whole
    // formation drops a row and reverses when it would cross an edge.
    // The pause between steps shrinks as aliens die.
    void step(Sound& sfx) {
        if (mStepCooldown > 0) { --mStepCooldown; return; }
        int living = aliveCount();
        mStepCooldown = stepInterval(living);

        int minX = PLAYFIELD_W, maxX = -1;
        for (int r = 0; r < SWARM_ROWS; ++r)
            for (int c = 0; c < SWARM_COLS; ++c) {
                Alien* a = mGrid[r][c];
                if (!a || !a->alive()) continue;
                if (a->posX() < minX) minX = a->posX();
                if (a->posX() > maxX) maxX = a->posX();
            }
        if (maxX < 0) return;   // nobody left

        bool drop = (mDx > 0 && maxX + mDx + ALIEN_WIDTH > PLAYFIELD_W) ||
                    (mDx < 0 && minX + mDx < 0);
        mAnimFrame = 1 - mAnimFrame;
        for (int r = 0; r < SWARM_ROWS; ++r)
            for (int c = 0; c < SWARM_COLS; ++c) {
                Alien* a = mGrid[r][c];
                if (!a || !a->alive()) continue;
                if (drop) a->move(0, SPRITE_H);
                else      a->move(mDx, 0);
                a->setFrame(mAnimFrame);
            }
        if (drop) mDx = -mDx;
        sfx.play(AssetIds::SOUND_MARCH_BASE + marchSpeed(living));
    }

    // 0 (full formation) .. 6 (the last alien): which march sound, and
    // how fast the formation steps
    int marchSpeed(int living) const {
        if (living > 40) return 0;
        if (living > 30) return 1;
        if (living > 20) return 2;
        if (living > 10) return 3;
        if (living >  5) return 4;
        if (living >  1) return 5;
        return 6;
    }

    // frames between march steps
    int stepInterval(int living) const {
        const int FRAMES[7] = { 30, 24, 18, 12, 8, 4, 2 };
        int frames = FRAMES[marchSpeed(living)];
        if (mDifficulty == DIFF_EASY)   frames += 12;
        if (mDifficulty == DIFF_MEDIUM) frames += 6;
        return frames;
    }

    Alien* mGrid[SWARM_ROWS][SWARM_COLS];
    int    mDx = 2;
    int    mAnimFrame = 0;
    int    mStepCooldown = 0;
    GameDifficulty mDifficulty = DIFF_MEDIUM;
};

} // namespace si
