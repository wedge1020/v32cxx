// *****************************************************************************
//  tests/113sample.cpp — three small gaps found adding power-ups to the
//  Space Invaders demo
//
//   1. An enum constant passed where an overloaded function wants an int:
//      `Vec2(SPREAD, 0)` with both `Vec2()` and `Vec2(int, int)` declared
//      matched no overload, because the argument's type is the enum.
//      (With a single candidate the call was accepted on argument count
//      alone, which is why it went unnoticed.) An enum value converts to
//      int, as in C++ -- but only when nothing matches exactly: with
//      f(Kind) and f(int) both declared, an enum argument picks f(Kind).
//   2. A ternary whose first branch reads a `const` table:
//      `flash ? COLORS[kind] : 7`. Vircon32 C has no `?:`, so the ternary
//      becomes a temporary assigned in an if/else -- and the temporary
//      was declared `const int`, like the table, then assigned to.
//   3. A ternary in a member-initializer list, `: energy(mega ? 6 : 1)`,
//      was reported as a `?:` outside a function body.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;

enum Tuning { SPREAD = 6, DRIFT = 2 };
enum Kind { KIND_A, KIND_B, KIND_COUNT };

const int COLORS[KIND_COUNT] = { 11, 22 };

struct Vec2 {
    int x;
    int y;
    Vec2() : x(0), y(0) {}
    Vec2(int px, int py) : x(px), y(py) {}
    Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
};

class Shot {
public:
    bool mega;
    int  energy;
    Shot(bool isMega) : mega(isMega), energy(isMega ? 6 : 1) {}
};

int scale(int n)      { return n * 10; }
int scale(int n, int m) { return n * m; }

// an overload that takes the enum itself wins over the int one
int name(Kind k) { return 1; }
int name(int n)  { return 2; }

int pick(bool flash, Kind kind)
{
    int a = flash ? COLORS[kind] : 7;
    int b = flash ? 7 : COLORS[kind];
    return a * 100 + b;
}

int main()
{
    int errors = 0;

    Vec2 a(SPREAD, 0);
    Vec2 b = Vec2(1, 1) + Vec2(DRIFT, SPREAD);
    Kind k = KIND_B;
    if (a.x != 6 || a.y != 0 || b.x != 3 || b.y != 7) errors++;
    if (scale(SPREAD) != 60 || scale(DRIFT, SPREAD) != 12 || scale(k) != 10) errors++;

    if (name(k) != 1 || name(KIND_A) != 1 || name(3) != 2) errors++;

    if (pick(true, KIND_B) != 2207 || pick(false, KIND_A) != 711) errors++;

    Shot plain(false);
    Shot big(true);
    if (plain.energy != 1 || big.energy != 6 || !big.mega) errors++;

    test_errors = errors;
    return 0;
}
