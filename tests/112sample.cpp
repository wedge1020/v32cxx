// *****************************************************************************
//  tests/112sample.cpp — three class features the Space Invaders demo had
//  been writing around
//
//   1. Pure virtual functions: `virtual int area() = 0;`. The vtable slot
//      gets a do-nothing body (returning 0 / a null pointer); a derived
//      class overrides it as usual.
//   2. Member-initializer lists for members that are objects:
//      `Bullet(...) : mVel(vx, vy), mSprite(s) {}` calls Vec2's two-int
//      constructor on the member, instead of default-constructing it and
//      setting it in the body. A member left out of the list is still
//      default-constructed; a call in an initializer's arguments works.
//   3. In-class default member initializers: `int mLives = 3;`. Applied
//      by every constructor that does not initialize the member itself,
//      including a constructor defined outside the class, and by the
//      implicit constructor of a class that declares none. (These used
//      to parse and then be silently ignored.)
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;
int g_vec_ctors = 0;

int twice(int n) { return n * 2; }

enum Kind { KIND_NONE, KIND_SQUARE };

// ---- 1. pure virtuals -------------------------------------------------------
class Shape
{
public:
    virtual int   area() const = 0;
    virtual void  grow(int by) = 0;
    virtual Kind  kind() const = 0;
    virtual Shape *self() = 0;
    virtual ~Shape() {}
};

class Square : public Shape
{
public:
    int side;
    Square(int s) : side(s) {}
    int   area() const { return side * side; }
    void  grow(int by) { side += by; }
    Kind  kind() const { return KIND_SQUARE; }
    Shape *self()      { return this; }
};

// ---- 2. class-typed members in the initializer list -------------------------
class Vec2
{
public:
    int x;
    int y;
    Vec2() : x(0), y(0)             { g_vec_ctors++; }
    Vec2(int px, int py) : x(px), y(py) { g_vec_ctors++; }
};

class Bullet
{
public:
    Vec2 pos;
    Vec2 vel;
    Vec2 spare;        // not in the list: default-constructed
    int  sprite;
    Bullet(int px, int py, int vy, int s)
        : pos(px, py), vel(0, twice(vy)), sprite(s) {}
};

// ---- 3. default member initializers -----------------------------------------
class Player
{
public:
    int  lives = 3;
    int  speed = twice(4);
    bool firing = false;
    int  x;
    Player()              { x = 0; }
    Player(int px)        : lives(5) { x = px; }      // lives: the list wins
    Player(int px, int lives);                        // defined below
};

Player::Player(int px, int lives) { x = px; this->speed = lives; }

class Settings          // no constructor of its own
{
public:
    int volume = 10;
    int music  = 6;
};

int main()
{
    int errors = 0;

    Square sq(3);
    Shape *s = &sq;
    if (s->area() != 9) errors++;
    s->grow(2);
    if (s->area() != 25 || s->kind() != KIND_SQUARE || s->self() != s) errors++;

    g_vec_ctors = 0;
    Bullet b(10, 20, -4, 7);
    if (b.pos.x != 10 || b.pos.y != 20) errors++;
    if (b.vel.x != 0 || b.vel.y != -8) errors++;
    if (b.spare.x != 0 || b.spare.y != 0 || b.sprite != 7) errors++;
    if (g_vec_ctors != 3) errors++;            // one constructor call per member

    Player p0;
    if (p0.lives != 3 || p0.speed != 8 || p0.firing || p0.x != 0) errors++;
    Player p1(9);
    if (p1.lives != 5 || p1.speed != 8 || p1.x != 9) errors++;
    Player p2(1, 2);                           // parameter named like a member
    if (p2.lives != 3 || p2.speed != 2 || p2.x != 1) errors++;
    Settings st;
    if (st.volume != 10 || st.music != 6) errors++;
    Settings *heap = new Settings();
    if (heap->volume != 10) errors++;
    delete heap;

    test_errors = errors;
    return 0;
}
