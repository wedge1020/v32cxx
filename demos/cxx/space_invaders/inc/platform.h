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
    }

    // Blit one sprite (10x20) with the given ASCII-value sprite id.
    void blit(int spriteId, int x, int y) {
        select_region(spriteId);
        draw_region_at(x, y);
    }

    // Clear the framebuffer.
    void clear() {
        clear_screen(color_black);
    }

    // Signal end of processing for this frame.
    void sync() {
        end_frame();
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
