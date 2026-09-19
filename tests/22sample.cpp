// Exercises lowering phase 7 (constructor invocation for stack-allocated
// locals) -- this is this project's own copy of a genuinely
// hand-written, real-world test (not an artificial unit test) that
// found the gap phase 7 closes: `Player sprite;` used to compile but
// never call Player::Player(), leaving sprite.x/sprite.y as
// uninitialized stack garbage. See docs/DESIGN_NOTES.md for the full
// story.
//
// select_texture/select_region/draw_region_at are deliberately never
// declared anywhere in this file (they'd normally come from a Vircon32
// SDK header via #include, which this project's preprocessor gap drops
// silently) -- confirms calls to an unresolved free function still pass
// through unchanged rather than erroring, which is what let this file
// compile via v32c++ at all despite that gap.

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
