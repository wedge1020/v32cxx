// *****************************************************************************
//  tests/125sample.cpp — `using namespace`, using-declarations, `using X = T`
//
//  `using namespace game;` makes every name in game visible unqualified:
//  variables, enums and their values, typedefs, classes, and functions --
//  which overload with the global ones of the same name (pick(1) is
//  game::pick, pick(1, 2) is ::pick). `using game::gfx::Sprite;` brings in
//  one name; a directive inside a block lasts to the end of the block;
//  `using Points = int;` is a typedef. The real v32 and std headers are
//  used through directives at the end.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

#include <v32/keyboard.hpp>
#include <v32/math.hpp>
#include <vector>

int test_errors = -1;
namespace game
{
    int lives = 3;
    enum Mode { Easy, Hard };
    class Ship { public: int x; Ship() { x = 1; } int fire() { return x + 10; } };
    typedef int Score;
    int twice(int v) { return v * 2; }
    int pick(int a) { return 1; }
    namespace gfx { class Sprite { public: int id; Sprite(int i) { id = i; } }; int draw(int s) { return s + 100; } }
}
namespace other { int pick(float f) { return 2; } class Boat { public: int y; }; }
int pick(int a, int b) { return 3; }

using namespace game;
using game::gfx::Sprite;
using Points = int;

int in_block()
{
    using namespace game::gfx;
    int r = draw(5);           // gfx::draw, found through the block's directive
    Sprite s(4);
    return r + s.id;           // 109
}

int check_game()
{
    int e = 0;
    Ship ship;                 // game::Ship
    Mode m = Hard;
    Score sc = twice(lives);   // 6
    Sprite sp(7);              // through the using-declaration
    Points p = 5;
    if (ship.fire() != 11 || m != Hard || sc != 6 || sp.id != 7 || p != 5) e++;
    if (pick(1) != 1 || pick(1, 2) != 3) e++;   // game::pick and ::pick both visible: overloads
    if (in_block() != 109) e++;
    if (game::twice(2) != 4) e++;               // qualified still fine
    return e;
}

using namespace std;
using namespace v32;

Keyboard keyboard( SecondGamepadPort );

int main()
{
    int e = check_game();
    vector<int> v;                              // std::vector
    v.push_back( minimum( 3, 9 ) );             // v32::minimum
    if( v[0] != 3 || keyboard.port() != 1 || clamp( 15, 0, 10 ) != 10 ) e++;
    KeyEvent ev;
    if( keyboard.next_event( &ev ) ) e++;       // no keyboard attached
    test_errors = e;
    return 0;
}
