// *****************************************************************************
//  tests/108sample.cpp — std::vector<T>, the transpiler-level generic
//
//  Like std::array (tests/104sample.cpp), `std::vector<T>` is a fixed form
//  the transpiler expands into one ordinary class per element type
//  (vector_int, vector_Enemy, ...), generated from the text in
//  src/generic.c. Storage comes from malloc()/free().
//   1. push_back / pop_back / size / empty / capacity, growing past
//      several reallocations, including push_back of the vector's own
//      element (which moves when the buffer grows);
//   2. operator[], at, front, back, data, begin/end, reserve, clear;
//   3. resize(n) zero-fills, resize(n, value) fills, shrinking keeps the
//      front;
//   4. erase and insert through `begin() + i`, and the remove-while-
//      iterating loop games use for dead objects;
//   5. element types: int, float, a struct, an unnamed class object
//      (`push_back(Enemy(...))`), a pointer to a class with virtual
//      functions, a class with virtual functions held by value;
//   6. `a = b` copies the elements; a vector as a class member, a
//      reference parameter, a global, and on the heap with new/delete;
//   7. `using namespace std;` allowing the bare `vector<T>` spelling.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************
#include <vector>

int test_errors = -1;

struct Point { int x; int y; };

class Enemy
{
public:
    int x;
    int hp;
    Enemy(int px, int php) { x = px; hp = php; }
    bool dead() const { return hp <= 0; }
};

class Shape
{
public:
    int size;
    virtual int area() { return 0; }
};

class Square : public Shape
{
public:
    Square(int s) { size = s; }
    virtual int area() { return size * size; }
};

class Squad
{
public:
    std::vector<Enemy> members;
    int totalHp() {
        int t = 0;
        for (int i = 0; i < members.size(); i++) t += members[i].hp;
        return t;
    }
};

std::vector<int> g_scores;

int sum(std::vector<int> &v)
{
    int t = 0;
    for (int *it = v.begin(); it != v.end(); ++it) t += *it;
    return t;
}

void fillTo(std::vector<int> &v, int n)
{
    for (int i = 0; i < n; i++) v.push_back(i);
}

using namespace std;

int main()
{
    int errors = 0;

    // growth
    std::vector<int> v;
    if (!v.empty() || v.size() != 0 || v.capacity() != 0) errors++;
    fillTo(v, 100);
    if (v.size() != 100 || v.capacity() < 100 || v.empty()) errors++;
    if (sum(v) != 4950 || v[0] != 0 || v[99] != 99 || v.front() != 0 || v.back() != 99) errors++;
    v.pop_back();
    if (v.size() != 99 || v.back() != 98) errors++;

    // push_back of the vector's own element, exactly when it has to grow
    std::vector<int> own;
    own.push_back(7);
    while (own.size() < own.capacity()) own.push_back(1);
    own.push_back(own[0]);
    if (own.back() != 7) errors++;

    // element access is assignable
    v[1] = 50;
    v.at(2) += 10;
    v.front() = 5;
    if (v[0] != 5 || v[1] != 50 || v[2] != 12 || v.data()[1] != 50) errors++;

    // reserve and clear keep the storage
    v.clear();
    if (v.size() != 0 || v.capacity() < 100) errors++;
    v.reserve(500);
    if (v.capacity() != 500 || !v.empty()) errors++;

    // resize
    v.push_back(1); v.push_back(2); v.push_back(3);
    v.resize(6);
    if (v.size() != 6 || v[2] != 3 || v[3] != 0 || v[5] != 0) errors++;
    v.resize(8, 9);
    if (v.size() != 8 || v[6] != 9 || v[7] != 9) errors++;
    v.resize(2);
    if (v.size() != 2 || v[1] != 2) errors++;

    // erase and insert
    std::vector<int> e;
    for (int i = 0; i < 6; i++) e.push_back(i * 10);      // 0 10 20 30 40 50
    e.erase(e.begin() + 2);                               // 0 10 30 40 50
    if (e.size() != 5 || e[2] != 30 || e[4] != 50) errors++;
    e.erase(e.begin());                                   // 10 30 40 50
    e.erase(e.end() - 1);                                 // 10 30 40
    if (e.size() != 3 || e[0] != 10 || e[2] != 40) errors++;
    e.insert(e.begin() + 1, 20);                          // 10 20 30 40
    e.insert(e.begin(), 0);                               // 0 10 20 30 40
    e.insert(e.end(), 50);                                // 0 10 20 30 40 50
    if (e.size() != 6) errors++;
    for (int i = 0; i < 6; i++) if (e[i] != i * 10) errors++;

    // floats, plain structs
    std::vector<float> f;
    f.push_back(1.5);
    f.push_back(2.5);
    if (f[0] + f[1] != 4.0) errors++;
    std::vector<Point> pts;
    Point p; p.x = 3; p.y = 4;
    pts.push_back(p);
    p.x = 30;
    pts.push_back(p);
    pts[0].y = 5;
    if (pts[0].x != 3 || pts[0].y != 5 || pts[1].x != 30 || pts.back().y != 4) errors++;

    // unnamed class objects; the remove-dead loop
    Squad squad;
    squad.members.push_back(Enemy(10, 3));
    squad.members.push_back(Enemy(20, 0));
    squad.members.push_back(Enemy(30, 5));
    squad.members.push_back(Enemy(40, 0));
    if (squad.members.size() != 4 || squad.totalHp() != 8) errors++;
    for (int i = 0; i < squad.members.size(); ) {
        if (squad.members[i].dead()) squad.members.erase(squad.members.begin() + i);
        else i++;
    }
    if (squad.members.size() != 2 || squad.members[0].x != 10 || squad.members[1].x != 30) errors++;
    Enemy &firstOne = squad.members.front();
    firstOne.hp = 1;
    if (squad.totalHp() != 6) errors++;

    // pointers to a class with virtual functions; the same class by value
    Square small(2);
    Square big(5);
    std::vector<Shape *> shapes;
    shapes.push_back(&small);
    shapes.push_back(&big);
    int area = 0;
    for (int i = 0; i < shapes.size(); i++) area += shapes[i]->area();
    if (area != 29) errors++;
    std::vector<Square> squares;
    squares.push_back(small);
    squares.push_back(Square(3));
    if (squares[0].area() + squares[1].area() != 13) errors++;

    // `a = b` copies the elements
    std::vector<int> a;
    std::vector<int> b;
    a.push_back(1); a.push_back(2);
    b.push_back(99);
    b = a;
    b[0] = 7;
    if (a[0] != 1 || b[0] != 7 || b.size() != 2 || b[1] != 2) errors++;

    // a global, and one on the heap
    if (g_scores.size() != 0) errors++;
    g_scores.push_back(100);
    g_scores.push_back(250);
    if (sum(g_scores) != 350) errors++;
    std::vector<int> *heap = new std::vector<int>();
    heap->push_back(4);
    heap->push_back(6);
    if (heap->size() != 2 || sum(*heap) != 10) errors++;
    delete heap;

    // the bare spelling, after `using namespace std;`
    vector<int> bare;
    bare.push_back(3);
    if (bare.back() != 3) errors++;

    test_errors = errors;
    return 0;
}
