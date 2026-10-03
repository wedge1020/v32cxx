// *****************************************************************************
//  tests/111sample.cpp — containers of types declared in a namespace
//
//  `std::vector<Bullet *>` written inside `namespace si`, where Bullet is
//  declared: the generated class has to be parsed and placed inside that
//  same namespace, next to its first use, or `Bullet` means nothing to it.
//  (Found converting the Space Invaders demo, whose classes all live in
//  one namespace.)
//   1. vector and array members of a class in a namespace, with element
//      types from that namespace, used from inside and outside it;
//   2. `push_back(new T(...))`: a `new` expression bound to the
//      `const T &` parameter gets a temporary;
//   3. `fill(0)` on an array of pointers stores a null pointer;
//   4. range-based for and virtual calls through the elements.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************
#include <array>
#include <vector>

int test_errors = -1;

namespace si {

class Bullet
{
public:
    int y;
    Bullet(int py) { y = py; }
    virtual int weight() { return 1; }
};

class Bomb : public Bullet
{
public:
    Bomb(int py) : Bullet(py) {}
    virtual int weight() { return 10; }
};

class Game
{
public:
    std::vector<Bullet *>   shots;
    std::array<Bullet *, 3> slots;

    Game()  { slots.fill(0); }
    ~Game() { for (Bullet *b : shots) delete b; }

    void fire(int y) { shots.push_back(new Bullet(y)); }
    void drop(int y) { shots.push_back(new Bomb(y)); }

    int total() {
        int t = 0;
        for (Bullet *b : shots) t += b->y * b->weight();
        return t;
    }
    int filled() {
        int n = 0;
        for (Bullet *b : slots) if (b) n++;
        return n;
    }
};

} // namespace si

int main()
{
    int errors = 0;

    si::Game game;
    if (game.filled() != 0 || game.shots.size() != 0) errors++;

    game.fire(3);
    game.drop(4);
    game.fire(5);
    if (game.shots.size() != 3 || game.total() != 48) errors++;

    game.slots[1] = game.shots[1];
    if (game.filled() != 1 || game.slots[1]->weight() != 10) errors++;

    // erase while walking, as a game removes dead objects
    for (int i = 0; i < game.shots.size(); ) {
        if (game.shots[i]->weight() == 10) {
            delete game.shots[i];
            game.shots.erase(game.shots.begin() + i);
        } else {
            i++;
        }
    }
    if (game.shots.size() != 2 || game.total() != 8) errors++;

    test_errors = errors;
    return 0;
}
