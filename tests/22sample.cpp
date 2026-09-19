// Exercises lowering phase 7 (constructor invocation for stack-allocated
// locals) -- this is this project's own copy of a genuinely
// hand-written, real-world test (not an artificial unit test) that
// found the gap phase 7 closes: `Player sprite;` used to compile but
// never call Player::Player(), leaving sprite.x/sprite.y as
// uninitialized stack garbage. See docs/DESIGN_NOTES.md for the full
// story.
//
// select_texture/select_region/draw_region_at come from Vircon32's own
// video.h SDK header, `#include`d below -- this project's own
// preprocessor gap (no macro expansion, no #include RESOLUTION) means
// v32c++ itself never sees what these functions actually look like, but
// the `#include` line itself is still captured and re-emitted verbatim
// at the top of the generated C (lexer.l's own pass-through), so the
// REAL Vircon32 C compiler resolves them exactly as it would for
// hand-written C. Confirms v32c++'s own call resolution correctly
// leaves an unresolved free function's call unchanged rather than
// erroring (there's no declaration for it anywhere v32c++ itself can
// see), which is what lets this file transpile at all despite the
// preprocessor gap -- the real compilation check happens downstream,
// on the Vircon32 toolchain, once the include actually resolves.
#include "video.h"

class Player {
    public:
        Player();
        void draw();
        void setx(int x);
        void sety(int y);

    private:
        int x;
        int y;
};

Player::Player() {
    this->x = 320;
    this->y = 180;
}

void Player::draw() {
    draw_region_at(this->x, this->y);
}

void Player::setx(int x) {
    this->x = x;
}

void Player::sety(int y) {
    this->y = y;
}

void main() {
    Player sprite;
    sprite.setx(320);
    sprite.sety(180);

    select_texture(-1);
    select_region(65);

    sprite.draw();
}
