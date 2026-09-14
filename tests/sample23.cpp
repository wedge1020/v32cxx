// Exercises real `new` allocation + constructor invocation (lower.c's
// per-constructor allocator naming + codegen.c's emit_new_delete_runtime)
// -- this project's own copy of Matthew's hand-written `sprite.cpp`, the
// `new`-based sibling of tests/sample22.cpp's `sprite2.cpp`. Player has
// an actual constructor BODY (unlike tests/sample17.cpp's Widget or
// sample18.cpp's Point, both prototype-only), so this is the only
// existing test that exercises the "allocate via malloc(), then
// actually call the constructor" path rather than the "no constructor
// has a body, just allocate" fallback.
//
// select_texture/select_region/draw_region_at are deliberately never
// declared anywhere in this file, same reasoning as sample22.cpp.

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
    Player *sprite = new Player();
    sprite->setx(320);
    sprite->sety(180);

    select_texture(-1);
    select_region(65);

    sprite->draw();
}
