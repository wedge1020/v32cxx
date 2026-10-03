#pragma once
// *****************************************************************************
//  game.hpp — the Game: owns every object and runs one frame at a time
// *****************************************************************************
#include <array>
#include <vector>
#include "entities.hpp"
#include "swarm.hpp"
#include "text.hpp"

namespace si {

enum GameState {
    GAME_TITLE,
    GAME_PLAYING,
    GAME_WAVE_CLEAR,
    GAME_OVER
};

// rows of the pause menu
enum PauseRow {
    PAUSE_GLOBAL_VOLUME,
    PAUSE_MUSIC_VOLUME,
    PAUSE_MUSIC_SWITCH,
    PAUSE_ROWS
};

class Game {
public:
    Game() {
        // none of these ever holds more, so none ever has to grow
        mBombs.reserve(BOMB_CAPACITY);
        mShots.reserve(3);
        mPowerUps.reserve(POWERUP_CAPACITY);
        mSfx.init();
        resetPlayer();
    }

    void runFrame(const Input& in) {
        updateMusic();
        if (mPaused) { pauseFrame(in); return; }   // the simulation is frozen
        switch (mState) {
            case GAME_TITLE:      titleFrame(in);    break;
            case GAME_PLAYING:    playFrame(in);     break;
            case GAME_WAVE_CLEAR: waveClearFrame();  break;
            default:              gameOverFrame(in); break;
        }
    }

    void draw(Video& video) {
        video.clear();
        if (mState == GAME_TITLE) {
            drawBlockTitle(video, mTitleTick);
            drawText(video, "EASY",   186, 150);
            drawText(video, "MEDIUM", 186, 170);
            drawText(video, "HARD",   186, 190);
            drawText(video, ">",      176, 150 + mDifficulty * 20);
            drawText(video, "PRESS START", 154, 220);
        } else {
            mPlayer.draw(video);
            mSwarm.draw(video);
            mSaucer.draw(video);
            for (Bunker& bunker : mBunkers) bunker.draw(video);
            for (Bullet& shot : mShots) shot.draw(video);
            for (Bullet& bomb : mBombs) bomb.draw(video);
            for (PowerUp& capsule : mPowerUps) capsule.draw(video);
            drawHUD(video);
        }
        if (mPaused) drawPauseOverlay(video);   // on top of the frozen scene
    }

private:
    // ---- title ---------------------------------------------------------------
    void titleFrame(const Input& in) {
        ++mTitleTick;   // drives the logo's wave
        if (in.pressed(BTN_UP) && mDifficulty > DIFF_EASY) {
            mDifficulty = (GameDifficulty)(mDifficulty - 1);
            mSfx.play(AssetIds::SOUND_MENU_MOVE);
        }
        if (in.pressed(BTN_DOWN) && mDifficulty < DIFF_HARD) {
            mDifficulty = (GameDifficulty)(mDifficulty + 1);
            mSfx.play(AssetIds::SOUND_MENU_MOVE);
        }
        if (in.pressed(BTN_START)) {
            mSfx.play(AssetIds::SOUND_MENU_SELECT);
            startNewGame();
        }
    }

    // ---- difficulty tuning -----------------------------------------------------
    // frames between bomb drops, before the random jitter
    int bombCooldownBase() const {
        if (mDifficulty == DIFF_EASY)   return 80;
        if (mDifficulty == DIFF_MEDIUM) return 55;
        return 30;
    }
    // the jitter window is wider on easier settings: less spiky, not just slower
    int bombCooldownJitter() const {
        if (mDifficulty == DIFF_EASY)   return 70;
        if (mDifficulty == DIFF_MEDIUM) return 55;
        return 45;
    }
    // fastest a bomb may fall, px per frame
    int bombFallSpeed() const {
        if (mDifficulty == DIFF_EASY)   return 1;
        if (mDifficulty == DIFF_MEDIUM) return 2;
        return 3;
    }
    // hit points a bomb takes off each bunker cell it touches (a cell has 4)
    int bunkerDamage() const {
        if (mDifficulty == DIFF_EASY)   return 1;
        if (mDifficulty == DIFF_MEDIUM) return 2;
        return 4;
    }
    // how long a weapon power-up lasts, in frames: no limit on easy (0),
    // 60 seconds on medium, 30 on hard
    int weaponFrames() const {
        if (mDifficulty == DIFF_EASY)   return 0;
        if (mDifficulty == DIFF_MEDIUM) return 60 * 60;
        return 30 * 60;
    }
    // bomb hits a shield power-up absorbs
    int shieldHits() const {
        if (mDifficulty == DIFF_EASY)   return 5;
        if (mDifficulty == DIFF_MEDIUM) return 3;
        return 1;
    }

    // ---- starting games and waves ------------------------------------------------
    void resetPlayer() {
        mPlayer.reset(PLAYFIELD_W / 2 - PLAYER_WIDTH / 2, PLAYER_HOME_Y);
    }

    void startNewGame() {
        mScore = 0;
        mHiScore = 0;
        mWave = 1;
        mPaused = false;
        mWaveClearTimer = 0;
        mLastExtraLifeAt = 0;
        mBombCooldown = bombCooldownBase();
        stopSaucerSound();
        mSwarm.setDifficulty(mDifficulty);
        resetPlayer();
        mBombs.clear();
        mShots.clear();
        mPowerUps.clear();
        mFireCooldown = 0;
        buildWave();
        mState = GAME_PLAYING;
#ifdef SI_TEST_POWERUPS
        // Test build (v32c++ -D SI_TEST_POWERUPS): one capsule of every
        // kind falls onto the cannon, 150 frames apart, so each power-up
        // can be seen without waiting on luck.
        for (int kind = 0; kind < POWER_KINDS; ++kind) {
            Vec2 above = mPlayer.muzzle() + Vec2(0, -60 - 150 * kind);
            mPowerUps.push_back(PowerUp(above, (PowerUpKind)(POWER_KINDS - 1 - kind)));
        }
#endif
    }

    void buildWave() {
        // Every wave starts at the same height: difficulty comes from the
        // march and bomb cadence, not from where the formation begins.
        mSwarm.spawn(40);
        rebuildBunkers();
    }

    // four 60 px bunkers across the 448 px field: x = 41, 143, 245, 347
    // (41 px margins, 42 px gaps), every cell back to full strength
    void rebuildBunkers() {
        for (int i = 0; i < mBunkers.size(); ++i) mBunkers[i].rebuild(41 + i * 102);
    }

    // ---- one frame of play ---------------------------------------------------------
    void playFrame(const Input& in) {
        if (in.pressed(BTN_START)) {
            mPaused = true;
            return;
        }
        mPlayer.handleInput(in);

        updateShots();
        mSwarm.update(mSfx);
        updateBombs();
        mSaucer.update();
        mPlayer.update();
        updateSaucerSound();
        checkCollisions();
        updatePowerUps();
        awardExtraLife();

        if (mPlayer.gameOver())                       mState = GAME_OVER;
        else if (mSwarm.aliveCount() == 0)            mState = GAME_WAVE_CLEAR;
        else if (mSwarm.reachedBottom(INVASION_Y))    mState = GAME_OVER;
    }

    // The cannon fires when nothing it fired is still in flight -- one
    // volley on screen at a time, whatever the weapon. A volley is one,
    // two or three ordinary shots, or a single mega bolt (which also
    // needs a moment to recharge after it is gone).
    void updateShots() {
        if (mFireCooldown > 0) --mFireCooldown;
        if (mPlayer.wantsFire() && mShots.empty() && mFireCooldown == 0) fire();
        mPlayer.clearFire();
        for (int i = 0; i < mShots.size(); ) {
            mShots[i].update();
            if (mShots[i].state() == STATE_DEAD) mShots.erase(mShots.begin() + i);
            else ++i;
        }
    }

    void fire() {
        Vec2 from = mPlayer.muzzle();
        Vec2 up(0, -SHOT_SPEED);
        switch (mPlayer.weapon()) {
            case WEAPON_DOUBLE:
                mShots.push_back(Bullet(from - Vec2(DOUBLE_SHOT_SPREAD, 0), up, AssetIds::PLAYER_BULLET));
                mShots.push_back(Bullet(from + Vec2(DOUBLE_SHOT_SPREAD, 0), up, AssetIds::PLAYER_BULLET));
                break;
            case WEAPON_TRIPLE:
                mShots.push_back(Bullet(from, up - Vec2(TRIPLE_SHOT_DRIFT, 0), AssetIds::PLAYER_BULLET));
                mShots.push_back(Bullet(from, up,                              AssetIds::PLAYER_BULLET));
                mShots.push_back(Bullet(from, up + Vec2(TRIPLE_SHOT_DRIFT, 0), AssetIds::PLAYER_BULLET));
                break;
            case WEAPON_MEGA:
                mShots.push_back(Bullet(from, Vec2(0, -MEGA_SHOT_SPEED), AssetIds::PLAYER_BULLET, true));
                mFireCooldown = MEGA_RECHARGE_FRAMES;
                break;
            default:
                mShots.push_back(Bullet(from, up, AssetIds::PLAYER_BULLET));
                break;
        }
        mSfx.play(AssetIds::SOUND_SHOOT);
    }

    // The aliens' bombs: Bullet values in a std::vector, at most
    // BOMB_CAPACITY at once.
    void updateBombs() {
        --mBombCooldown;
        if (mBombCooldown <= 0 && mBombs.size() < BOMB_CAPACITY) {
            Alien* shooter = mSwarm.randomShooter();
            if (shooter) {
                // falls faster in later waves, up to the difficulty's cap
                int speed = 1 + mWave / 3;
                if (speed > bombFallSpeed()) speed = bombFallSpeed();
                Vec2 from(shooter->posX() + ALIEN_WIDTH / 2 - SPRITE_W / 2,
                          shooter->posY() + ALIEN_HEIGHT);
                mBombs.push_back(Bullet(from, Vec2(0, speed),
                                        g_rng.coin() ? AssetIds::ALIEN_BULLET_SQUIGGLE
                                                     : AssetIds::ALIEN_BULLET_PLUMB));
            }
            mBombCooldown = bombCooldownBase() + g_rng.next(bombCooldownJitter());
        }
        // an index loop, not a range-based for: bombs are erased on the way
        for (int i = 0; i < mBombs.size(); ) {
            mBombs[i].update();
            if (mBombs[i].state() == STATE_DEAD) mBombs.erase(mBombs.begin() + i);
            else ++i;
        }
    }

    // ---- collisions -------------------------------------------------------------------
    void checkCollisions() {
        // the cannon's shots: aliens first, then the saucer, then the bunkers
        for (int i = 0; i < mShots.size(); ) {
            if (shotHitSomething(mShots[i])) mShots.erase(mShots.begin() + i);
            else ++i;
        }
        // the bombs: the cannon first, then the bunkers
        for (int i = 0; i < mBombs.size(); ) {
            bool gone = false;
            if (mPlayer.collidesWith(mBombs[i])) {
                mPlayer.hit(mSfx);
                gone = true;
            } else {
                Rect bomb = mBombs[i].bounds();
                for (Bunker& bunker : mBunkers) {
                    if (bunker.erode(bomb, mSfx, bunkerDamage())) {
                        gone = true;
                        break;
                    }
                }
            }
            if (gone) mBombs.erase(mBombs.begin() + i);
            else ++i;
        }
    }

    // What one shot hit this frame. Returns true if the shot is used up.
    // An ordinary shot is used up by the first thing it touches; a mega
    // bolt spends one of its six hits per target and flies on, so it
    // takes a whole column of aliens and can still reach the saucer.
    bool shotHitSomething(Bullet& shot) {
        Rect box = shot.bounds();
        Alien* a = mSwarm.hitTest(box);
        while (a) {
            addScore(a->points());
            a->destroy(mSfx);
            if (g_rng.next(POWERUP_ODDS) == 0) dropPowerUp(Vec2(a->posX(), a->posY()));
            if (shot.spend()) return true;
            a = mSwarm.hitTest(box);     // two aliens under one wide bolt
        }
        if (mSaucer.flying() && mSaucer.collidesWith(box)) {
            addScore(mSaucer.scoreValue());
            mSaucer.destroy(mSfx);
            // the saucer always leaves a capsule behind
            dropPowerUp(Vec2(mSaucer.posX() + SAUCER_WIDTH / 2 - SPRITE_W / 2, mSaucer.posY()));
            if (shot.spend()) return true;
        }
        // Bunkers stop an ordinary shot. A mega bolt burns straight
        // through, destroying the cells in its way, at no cost to itself.
        for (Bunker& bunker : mBunkers) {
            if (bunker.erode(box, mSfx, shot.mega() ? BUNKER_CELL_HP : 1) && !shot.mega()) return true;
        }
        return false;
    }

    // ---- power-ups ------------------------------------------------------------------------
    void dropPowerUp(const Vec2& at) {
        if (mPowerUps.size() >= POWERUP_CAPACITY) return;
        mPowerUps.push_back(PowerUp(at, (PowerUpKind)g_rng.next(POWER_KINDS)));
    }

    void updatePowerUps() {
        for (int i = 0; i < mPowerUps.size(); ) {
            mPowerUps[i].update();
            bool caught = mPlayer.alive() && mPlayer.collidesWith(mPowerUps[i]);
            if (caught) collect(mPowerUps[i].kind());
            if (caught || mPowerUps[i].state() == STATE_DEAD) mPowerUps.erase(mPowerUps.begin() + i);
            else ++i;
        }
    }

    void collect(PowerUpKind kind) {
        switch (kind) {
            case POWER_DOUBLE: mPlayer.giveWeapon(WEAPON_DOUBLE, weaponFrames()); break;
            case POWER_TRIPLE: mPlayer.giveWeapon(WEAPON_TRIPLE, weaponFrames()); break;
            case POWER_MEGA:   mPlayer.giveWeapon(WEAPON_MEGA,   weaponFrames()); break;
            case POWER_BLAST:  addScore(mSwarm.destroyBottomRow(mSfx));           break;
            case POWER_REPAIR: rebuildBunkers();                                  break;
            default:           mPlayer.giveShield(shieldHits());                  break;
        }
        mSfx.play(AssetIds::SOUND_EXTRA_LIFE);
    }

    // ---- pause menu ----------------------------------------------------------------------
    // START freezes the game. UP/DOWN pick a row, LEFT/RIGHT change a
    // volume, A flips the music switch, START resumes.
    void pauseFrame(const Input& in) {
        if (in.pressed(BTN_START)) {
            mPaused = false;
            mSfx.play(AssetIds::SOUND_MENU_SELECT);
            return;
        }
        if (in.pressed(BTN_UP) && mPauseRow > 0) {
            --mPauseRow;
            mSfx.play(AssetIds::SOUND_MENU_MOVE);
        }
        if (in.pressed(BTN_DOWN) && mPauseRow < PAUSE_ROWS - 1) {
            ++mPauseRow;
            mSfx.play(AssetIds::SOUND_MENU_MOVE);
        }
        int change = 0;
        if (in.pressed(BTN_LEFT))  change = -1;
        if (in.pressed(BTN_RIGHT)) change = 1;
        if (change != 0 && mPauseRow != PAUSE_MUSIC_SWITCH) {
            bool global = (mPauseRow == PAUSE_GLOBAL_VOLUME);
            int wanted = (global ? mVolGlobal : mVolMusic) + change;
            if (wanted >= 0 && wanted <= 10) {
                if (global) {
                    mVolGlobal = wanted;
                    mSfx.setGlobalVolume(mVolGlobal);
                } else {
                    mVolMusic = wanted;
                    applyMusicVolume();
                }
                mSfx.play(AssetIds::SOUND_MENU_MOVE);
            }
        }
        if (in.pressed(BTN_A) && mPauseRow == PAUSE_MUSIC_SWITCH) {
            mMusicOn = !mMusicOn;
            mSfx.play(AssetIds::SOUND_MENU_SELECT);
        }
    }

    void drawPauseOverlay(Video& video) {
        // dim the frozen scene behind a wall of dark blocks
        video.tint(color_darkgray);
        for (int y = 0; y < PLAYFIELD_H; y += SPRITE_H)
            for (int x = 0; x < PLAYFIELD_W; x += SPRITE_W)
                video.blit(AssetIds::BUNKER_BLOCK_4, x, y);

        drawText(video, "PAUSED", 194, 40);
        drawText(video, "GLOBAL VOLUME", 114, 100);
        drawText(video, "MUSIC VOLUME",  114, 120);
        drawText(video, mMusicOn ? "GAMEPLAY MUSIC ON" : "GAMEPLAY MUSIC OFF", 114, 140);
        drawText(video, ">", 100, 100 + mPauseRow * 20);
        drawVolumeBar(video, mVolGlobal, 100);
        drawVolumeBar(video, mVolMusic,  120);
        drawText(video, "START RESUMES  A TOGGLES MUSIC", 44, 200);
    }

    // ten blocks to the right of a label, the first `level` of them lit
    void drawVolumeBar(Video& video, int level, int y) {
        for (int i = 0; i < 10; ++i) {
            video.tint(i < level ? color_white : color_darkgray);
            video.blit(AssetIds::BUNKER_BLOCK_4, 274 + i * SPRITE_W, y);
        }
    }

    // ---- music and the saucer's warble -------------------------------------------------------
    // One looping track per screen: the title theme, the gameplay theme
    // (which the pause menu can switch off), and silence on game over.
    // Started when the wanted track changes, stopped by channel.
    void updateMusic() {
        int wanted = AssetIds::SOUND_MUSIC_GAME;
        if (mState == GAME_TITLE)     wanted = AssetIds::SOUND_MUSIC_TITLE;
        else if (mState == GAME_OVER) wanted = -1;
        else if (!mMusicOn)           wanted = -1;
        if (wanted == mMusicId) return;
        if (mMusicChannel >= 0) {
            mSfx.stopChannel(mMusicChannel);
            mMusicChannel = -1;
        }
        mMusicId = wanted;
        if (mMusicId >= 0) {
            mMusicChannel = mSfx.play(mMusicId);   // -1 if no channel was free
            applyMusicVolume();                    // a new channel starts at full volume
        }
    }

    void applyMusicVolume() {
        if (mMusicChannel >= 0) mSfx.setChannelVolume(mMusicChannel, mVolMusic);
    }

    // The warble loops, so it is started once when the saucer appears and
    // stopped when it leaves or dies. If no channel was free, play()
    // returned -1 and this simply tries again next frame.
    void updateSaucerSound() {
        if (!mSaucer.flying()) stopSaucerSound();
        else if (mSaucerChannel < 0) mSaucerChannel = mSfx.play(AssetIds::SOUND_SAUCER);
    }

    void stopSaucerSound() {
        if (mSaucerChannel >= 0) mSfx.stopChannel(mSaucerChannel);
        mSaucerChannel = -1;
    }

    // ---- progression ----------------------------------------------------------------------------
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
        if (in.pressed(BTN_START)) mState = GAME_TITLE;
    }

    void addScore(int points) {
        mScore += points;
        if (mScore > mHiScore) mHiScore = mScore;
    }

    // one extra life each time the score crosses a multiple of 1500
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
        // top right: the weapon in hand (with its seconds left, if it is
        // timed) and the shield's remaining hits
        Weapon weapon = mPlayer.weapon();
        if (weapon != WEAPON_SINGLE) {
            drawText(video, weapon == WEAPON_DOUBLE ? "DOUBLE"
                          : weapon == WEAPON_TRIPLE ? "TRIPLE" : "MEGA", 250, 2);
            if (mPlayer.weaponSecondsLeft() > 0)
                drawNumber(video, mPlayer.weaponSecondsLeft(), 320, 2);
        }
        if (mPlayer.shield() > 0) {
            drawText(video, "SHIELD", 360, 2);
            drawNumber(video, mPlayer.shield(), 430, 2);
        }
        video.tint(color_green);   // spare cannons, bottom left
        for (int i = 0; i < mPlayer.lives() - 1; ++i)
            video.blit2(AssetIds::PLAYER_SHIP_TURRET, AssetIds::PLAYER_SHIP_BASE,
                        8 + i * 16, 236, PLAYER_TURRET_DROP);
    }

    // ---- everything the game owns -------------------------------------------------------------------
    Player  mPlayer;
    Swarm   mSwarm;
    Saucer  mSaucer;
    Sound   mSfx;
    std::vector<Bullet>  mShots;                   // the cannon's volley in flight
    std::vector<Bullet>  mBombs;                   // alien bombs in flight
    std::vector<PowerUp> mPowerUps;                // capsules falling
    std::array<Bunker, BUNKER_COUNT> mBunkers;
    int mFireCooldown = 0;                         // frames until the mega shot recharges

    GameState      mState = GAME_TITLE;
    GameDifficulty mDifficulty = DIFF_MEDIUM;
    int  mScore = 0;
    int  mHiScore = 0;
    int  mWave = 1;
    int  mBombCooldown = 60;
    int  mWaveClearTimer = 0;
    int  mLastExtraLifeAt = 0;
    int  mTitleTick = 0;

    bool mPaused = false;
    int  mPauseRow = PAUSE_GLOBAL_VOLUME;
    int  mVolGlobal = 10;        // tenths
    int  mVolMusic = 6;
    bool mMusicOn = true;        // gameplay music (the title theme always plays)

    int  mSaucerChannel = -1;    // channel of the saucer's warble, -1 = silent
    int  mMusicChannel = -1;     // channel of the current track, -1 = silent
    int  mMusicId = -1;          // sound id of the current track, -1 = none
};

} // namespace si
