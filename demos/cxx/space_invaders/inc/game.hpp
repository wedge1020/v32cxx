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
        mBombs.reserve(BOMB_CAPACITY);   // it never holds more: see updateBombs
        mSfx.init();
        resetPlayer();
    }

    ~Game() { delete mPlayerBullet; }

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
            if (mPlayerBullet) mPlayerBullet->draw(video);
            for (Bullet& bomb : mBombs) bomb.draw(video);
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
    int shieldDamage() const {
        if (mDifficulty == DIFF_EASY)   return 1;
        if (mDifficulty == DIFF_MEDIUM) return 2;
        return 4;
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
        deleteBullet();
        buildWave();
        mState = GAME_PLAYING;
    }

    void buildWave() {
        // Every wave starts at the same height: difficulty comes from the
        // march and bomb cadence, not from where the formation begins.
        mSwarm.spawn(40);
        // four 60 px bunkers across the 448 px field: x = 41, 143, 245, 347
        // (41 px margins, 42 px gaps)
        for (int i = 0; i < mBunkers.size(); ++i) mBunkers[i].rebuild(41 + i * 102);
    }

    // ---- one frame of play ---------------------------------------------------------
    void playFrame(const Input& in) {
        if (in.pressed(BTN_START)) {
            mPaused = true;
            return;
        }
        mPlayer.handleInput(in);

        updatePlayerBullet();
        mSwarm.update(mSfx);
        updateBombs();
        mSaucer.update();
        mPlayer.update();
        updateSaucerSound();
        checkCollisions();
        awardExtraLife();

        if (mPlayer.gameOver())                         mState = GAME_OVER;
        else if (mSwarm.aliveCount() == 0)              mState = GAME_WAVE_CLEAR;
        else if (mSwarm.reachedBottom(PLAYER_HOME_Y))   mState = GAME_OVER;
    }

    // The player has one shot on screen at a time: a Bullet on the heap,
    // or nullptr.
    void updatePlayerBullet() {
        if (mPlayer.wantsFire() && mPlayerBullet == nullptr) {
            mPlayerBullet = new Bullet(mPlayer.muzzle(), Vec2(0, -8),
                                       AssetIds::PLAYER_BULLET);
            mSfx.play(AssetIds::SOUND_SHOOT);
        }
        mPlayer.clearFire();
        if (mPlayerBullet) {
            mPlayerBullet->update();
            if (mPlayerBullet->state() == STATE_DEAD) deleteBullet();
        }
    }

    void deleteBullet() {
        delete mPlayerBullet;
        mPlayerBullet = nullptr;
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
        // the player's shot: aliens first, then the saucer, then the bunkers
        if (mPlayerBullet) {
            Rect shot = mPlayerBullet->bounds();
            Alien* a = mSwarm.hitTest(shot);
            if (a) {
                a->destroy(mSfx);
                addScore(a->points());
                deleteBullet();
            } else if (mSaucer.flying() && mSaucer.collidesWith(shot)) {
                addScore(mSaucer.scoreValue());
                mSaucer.destroy(mSfx);
                deleteBullet();
            } else {
                for (Bunker& bunker : mBunkers) {
                    if (bunker.erode(shot, mSfx, 1)) {
                        deleteBullet();
                        break;
                    }
                }
            }
        }
        // the bombs: the player first, then the bunkers
        for (int i = 0; i < mBombs.size(); ) {
            bool gone = false;
            if (mPlayer.collidesWith(mBombs[i])) {
                mPlayer.hit(mSfx);
                gone = true;
            } else {
                Rect bomb = mBombs[i].bounds();
                for (Bunker& bunker : mBunkers) {
                    if (bunker.erode(bomb, mSfx, shieldDamage())) {
                        gone = true;
                        break;
                    }
                }
            }
            if (gone) mBombs.erase(mBombs.begin() + i);
            else ++i;
        }
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
    Bullet* mPlayerBullet = nullptr;               // the one shot in flight, if any
    std::vector<Bullet> mBombs;                    // alien bombs in flight
    std::array<Bunker, BUNKER_COUNT> mBunkers;

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
