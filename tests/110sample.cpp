// *****************************************************************************
//  tests/110sample.cpp — range-based for, and inlined container accessors
//
//  `for (Enemy &e : enemies) body` is written out at parse time as the
//  pointer loop it stands for (parser.y), over anything with begin() and
//  end() returning `T *`: std::array, std::vector, or a class of your own.
//   1. by value (a copy), by reference (changes land in the container),
//      by const reference;
//   2. over std::array, std::vector, a container that is a class member,
//      one reached through a reference parameter, and a user class with
//      its own begin()/end();
//   3. pointer elements (`for (Shape *s : shapes)`), virtual calls inside;
//   4. break and continue; nested loops; an empty vector runs zero times;
//   5. a one-statement body without braces.
//
//  The second half checks the accessors the transpiler writes in place
//  instead of calling (lower.c, inline_container_accessor): v[i], at,
//  front, back, size, empty, capacity, data, begin, end -- with a
//  receiver that is a local, a member, `this->member`, a reference, a
//  pointer, and the result of a call (which must run exactly once).
//  Self-checking: test_errors must end at 0.
// *****************************************************************************
#include <array>
#include <vector>

int test_errors = -1;
int g_calls = 0;

struct Enemy { int x; int hp; };

class Shape
{
public:
    int size;
    virtual int area() { return size; }
};

class Square : public Shape
{
public:
    Square(int s) { size = s; }
    virtual int area() { return size * size; }
};

class Bag                       // a container of one's own
{
public:
    int items[4];
    int count;
    Bag() { count = 0; }
    void add(int v) { items[count] = v; count++; }
    int *begin() { return items; }
    int *end()   { return items + count; }
};

class World
{
public:
    std::vector<Enemy> enemies;
    std::array<int, 3> lanes;
    int totalHp() {
        int t = 0;
        for (const Enemy &e : enemies) t += e.hp;
        return t;
    }
    int lastLane()  { return this->lanes.back() + lanes.size(); }
    int enemyCount() { return enemies.size(); }
};

std::vector<int> g_list;

std::vector<int> &theList() { g_calls++; return g_list; }

int sumAll(std::vector<int> &v)
{
    int t = 0;
    for (int n : v) t += n;
    return t;
}

int main()
{
    int errors = 0;

    // ---- range-based for ----------------------------------------------------
    std::vector<int> v;
    for (int n : v) errors++;                    // empty: never runs
    for (int i = 1; i <= 5; i++) v.push_back(i);

    int total = 0;
    for (int n : v) total += n;
    if (total != 15 || sumAll(v) != 15) errors++;

    for (int &n : v) n *= 2;                     // by reference: 2 4 6 8 10
    if (v[0] != 2 || v[4] != 10) errors++;
    for (int n : v) n = 0;                       // by value: a copy
    if (v[0] != 2) errors++;

    total = 0;
    for (int n : v) {
        if (n == 4) continue;
        if (n == 8) break;
        total += n;                              // 2 + 6
    }
    if (total != 8) errors++;

    std::array<int, 3> a = {1, 2, 3};
    total = 0;
    for (int x : a)
        for (int y : a) total += x * y;          // (1+2+3) * (1+2+3)
    if (total != 36) errors++;

    World w;
    Enemy e; e.x = 0; e.hp = 5;
    w.enemies.push_back(e);
    e.hp = 7;
    w.enemies.push_back(e);
    for (Enemy &each : w.enemies) each.x = each.hp * 10;
    if (w.totalHp() != 12 || w.enemies[0].x != 50 || w.enemies[1].x != 70) errors++;

    Bag bag;
    bag.add(3); bag.add(4); bag.add(5);
    total = 0;
    for (int item : bag) total += item;
    if (total != 12) errors++;

    Square small(2);
    Square big(3);
    std::vector<Shape *> shapes;
    shapes.push_back(&small);
    shapes.push_back(&big);
    total = 0;
    for (Shape *s : shapes) total += s->area();
    if (total != 13) errors++;

    // ---- inlined accessors --------------------------------------------------
    if (v.size() != 5 || v.empty() || v.capacity() < 5) errors++;
    if (v.front() != 2 || v.back() != 10 || v.at(1) != 4 || v.data()[2] != 6) errors++;
    v.back() = 11;
    v.front()++;
    if (v[4] != 11 || v[0] != 3 || *v.begin() != 3 || *(v.end() - 1) != 11) errors++;

    w.lanes.fill(4);
    w.lanes[2] = 9;
    if (w.lastLane() != 12 || w.enemyCount() != 2) errors++;
    if (a.size() != 3 || a.empty() || a.front() != 1 || a.back() != 3 || a.data()[1] != 2) errors++;
    if (*a.begin() != 1 || *(a.end() - 1) != 3) errors++;

    std::vector<int> &ref = v;
    std::vector<int> *ptr = &v;
    if (ref.size() != 5 || ptr->size() != 5 || ref[1] != 4 || ptr->back() != 11) errors++;
    const std::vector<int> &cref = v;
    if (cref.size() != 5 || cref.empty()) errors++;

    // a receiver that is a call: the call runs exactly once each time
    g_list.push_back(8);
    g_list.push_back(9);
    g_calls = 0;
    if (theList().back() != 9) errors++;
    if (theList().size() != 2) errors++;
    if (theList()[0] != 8) errors++;
    if (g_calls != 3) errors++;

    test_errors = errors;
    return 0;
}
