#pragma once
// *****************************************************************************
//  platform.hpp — the three thin layers between the game and the console
//
//  Input, Video and Sound wrap the Vircon32 SDK's C functions. The .h
//  includes below are passed through to the generated C (v32c++ only reads
//  them for their names: color_green, gamepad_left, ...); everything the
//  game draws, hears or reads goes through these three classes.
// *****************************************************************************
#include "video.h"
#include "audio.h"
#include "input.h"
#include "time.h"    // end_frame()
#include "assets.hpp"

namespace si {

// ---------------------------------------------------------------------------
// Input. Every SDK button query returns a signed frame count that is never
// zero: +N = held for N frames, -N = released for N frames. So `== 1` means
// "pressed this very frame" and `> 0` means "down".
// ---------------------------------------------------------------------------
enum Button {
    BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
    BTN_START, BTN_A, BTN_B, BTN_X, BTN_Y, BTN_L, BTN_R
};

class Input {
public:
    int read(Button b) const {
        switch (b) {
            case BTN_UP:    return gamepad_up();
            case BTN_DOWN:  return gamepad_down();
            case BTN_LEFT:  return gamepad_left();
            case BTN_RIGHT: return gamepad_right();
            case BTN_START: return gamepad_button_start();
            case BTN_A:     return gamepad_button_a();
            case BTN_B:     return gamepad_button_b();
            case BTN_X:     return gamepad_button_x();
            case BTN_Y:     return gamepad_button_y();
            case BTN_L:     return gamepad_button_l();
            default:        return gamepad_button_r();
        }
    }
    bool pressed(Button b) const { return read(b) == 1; }   // this frame
    bool held(Button b) const    { return read(b) > 0; }
};

// ---------------------------------------------------------------------------
// Video. The game thinks in a 448 x 256 playfield; the screen is 640 x 360.
// Everything is scaled by 360/256 = 45/32 (1.40625), which fills the screen
// height and leaves a 5 px margin either side.
// ---------------------------------------------------------------------------
class Video {
public:
    void init() {
        select_texture(-1);                     // the BIOS font: one region per character
        set_drawing_scale(1.40625, 1.40625);
    }

    void clear() { clear_screen(color_black); }
    void sync()  { end_frame(); }

    // Multiply colour for everything drawn next (ABGR; white is neutral).
    void tint(int color) { set_multiply_color(color); }

    // One 10 x 20 glyph at playfield position (x, y).
    void blit(int spriteId, int x, int y) {
        select_region(spriteId);
        draw_region_zoomed_at(screenX(x), screenY(y));
    }

    // Two glyphs in the same cell, the first dropped dyA pixels toward the
    // second -- the player's '^' turret sitting on its '_' base.
    void blit2(int spriteIdA, int spriteIdB, int x, int y, int dyA = 0) {
        blit(spriteIdA, x, y + dyA);
        blit(spriteIdB, x, y);
    }

    // One glyph stretched: its 10 x 20 cell becomes (10 * sx) x (20 * sy),
    // growing right and down from (x, y). The saucer is built from these.
    void blitScaled(int spriteId, int x, int y, float sx, float sy) {
        set_drawing_scale(1.40625 * sx, 1.40625 * sy);
        blit(spriteId, x, y);
        set_drawing_scale(1.40625, 1.40625);
    }

private:
    int screenX(int x) const { return 5 + (x * 45) / 32; }
    int screenY(int y) const { return (y * 45) / 32; }
};

// ---------------------------------------------------------------------------
// Sound. play_sound() starts a sound in the first free channel and returns
// that channel (-1 if all 16 are busy); a looping sound is stopped by
// channel, so whoever starts one keeps the number.
// ---------------------------------------------------------------------------
class Sound {
public:
    void init() {
        setLooping(AssetIds::SOUND_SAUCER);
        setLooping(AssetIds::SOUND_MUSIC_TITLE);
        setLooping(AssetIds::SOUND_MUSIC_GAME);
    }

    int  play(int soundId)          { return play_sound(soundId); }
    void stopChannel(int channelId) { stop_channel(channelId); }

    // Volumes in steps of a tenth, 0..10.
    void setGlobalVolume(int tenths) { set_global_volume(tenths / 10.0); }
    void setChannelVolume(int channelId, int tenths) {
        select_channel(channelId);
        set_channel_volume(tenths / 10.0);
    }

private:
    void setLooping(int soundId) {
        select_sound(soundId);
        set_sound_loop(true);
    }
};

} // namespace si
