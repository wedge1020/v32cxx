// *****************************************************************************
//  tests/104sample.cpp — std::array<T, N>, the transpiler-level generic
//
//  v32c++ has no templates. `std::array<T, N>` is a fixed form the
//  transpiler knows: each distinct T/N pair becomes one ordinary class
//  (array_int_3, array_Enemy_4, ...), generated from the text in
//  src/generic.c and then treated like any hand-written class.
//   1. element types: int, float, a struct, a pointer, a class with a
//      constructor and destructor (every element is constructed and
//      destroyed), another std::array;
//   2. the length as a literal, a #define, and an enum constant;
//   3. operator[], at, front, back, size, empty, fill, data, begin/end;
//   4. aggregate initialization (`= {1, 2, 3}`), local and global;
//   5. as a class member, a reference parameter, a const reference
//      parameter, and copied whole with `=`;
//   6. `using namespace std;` allowing the bare `array<T, N>` spelling.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************
#include <array>

#define MAX_STARS 5
enum Limits { MAX_SHOTS = 6 };

struct Enemy { int x; int y; int hp; };

int g_live = 0;

class Counter {
public:
    int value;
    Counter()  { value = 3; g_live++; }
    ~Counter() { g_live--; }
};

class Wave {
public:
    std::array<Enemy, 4> slots;
    int alive() {
        int n = 0;
        for (int i = 0; i < slots.size(); i++) if (slots[i].hp > 0) n++;
        return n;
    }
};

std::array<int, 4> g_table = {10, 20, 30, 40};

int sum(std::array<int, 3> &a) {
    int t = 0;
    for (int i = 0; i < a.size(); i++) t += a[i];
    return t;
}

int sumConst(const std::array<int, 3> &a) {
    int t = 0;
    for (int i = 0; i < a.size(); i++) t += a[i];
    return t;
}

void zero(std::array<int, 3> &a) { a.fill(0); }

int test_errors = -1;

using namespace std;

int main() {
    int errors = 0;

    // ints: aggregate init, operator[], at, front/back, fill
    std::array<int, 3> a = {1, 2, 3};
    std::array<int, 3> b;
    b.fill(7);                 // a literal bound to fill's `const int &`
    b[1] = a[2] + 1;
    a.at(0) += 10;
    if (sum(a) != 16) errors++;
    if (sumConst(b) != 18) errors++;
    if (a.front() != 11 || a.back() != 3) errors++;
    if (a.size() != 3 || a.max_size() != 3 || a.empty()) errors++;

    // whole-array copy, then the copy is independent
    b = a;
    b[0] = 0;
    if (sum(b) != 5 || sum(a) != 16) errors++;
    zero(b);
    if (sum(b) != 0) errors++;

    // a global, and a length from a #define and from an enum
    if (g_table[3] != 40 || g_table.size() != 4) errors++;
    g_table[0] = g_table[1] + g_table[2];
    if (g_table.front() != 50) errors++;
    std::array<float, MAX_SHOTS> shots;
    shots.fill(1.5);
    if (shots.size() != 6 || shots[5] != 1.5) errors++;
    std::array<int, MAX_STARS> stars;
    stars.fill(2);
    if (stars.size() != 5 || stars.back() != 2) errors++;

    // structs, as a member of a class
    Wave w;
    Enemy e; e.x = 1; e.y = 2; e.hp = 5;
    w.slots.fill(e);
    w.slots[2].hp = 0;
    if (w.alive() != 3) errors++;
    int total = 0;
    for (Enemy *it = w.slots.begin(); it != w.slots.end(); ++it) total += it->hp;
    if (total != 15) errors++;
    Enemy &last = w.slots.back();
    last.x = 42;
    if (w.slots[3].x != 42 || w.slots.data()[3].x != 42) errors++;

    // pointers
    std::array<Enemy *, 2> ptrs;
    ptrs[0] = &e;
    ptrs[1] = &w.slots[0];
    ptrs[1]->hp = 9;
    if (w.slots.front().hp != 9 || ptrs[0]->hp != 5) errors++;

    // a class with a constructor and destructor: every element is
    // constructed, and destroyed when the array goes out of scope
    {
        std::array<Counter, 3> counters;
        if (g_live != 3 || counters[0].value != 3 || counters[2].value != 3) errors++;
    }
    if (g_live != 0) errors++;

    // an array of arrays
    std::array<std::array<int, 3>, 2> grid;
    grid[0].fill(1);
    grid[1].fill(2);
    grid[1][2] = 9;
    if (sum(grid[0]) != 3 || sum(grid[1]) != 13) errors++;

    // the bare spelling, after `using namespace std;`
    array<int, 2> pair = {4, 5};
    if (pair[0] + pair[1] != 9) errors++;

    test_errors = errors;
    return 0;
}
