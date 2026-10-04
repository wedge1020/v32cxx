// *****************************************************************************
//  tests/115sample.cpp — auto
//
//  `auto` for a local variable takes its type from the initializer, and
//  the range-based for accepts it for the element:
//
//      for (auto f : foes)          a copy of each element
//      for (auto &f : foes)         each element itself
//      for (const auto &f : foes)   each element, read-only
//      for (auto *p : pointers)     elements that are pointers
//
//  As in C++, a plain `auto` drops a reference and a top-level const.
//  `auto` is not supported for globals, members, parameters or return
//  types (each is an error that says so).
//
//  Self-checking: test_errors is 0 when everything passed.
// *****************************************************************************
#include <string>
#include <vector>
#include <array>
struct Enemy { int hp; int x; };
int test_errors = -1;
int twice(int v) { return v * 2; }
int main()
{
    int e = 0;
    std::vector<Enemy> foes;
    for (int i = 0; i < 3; i++) { Enemy f; f.hp = i + 1; f.x = 0; foes.push_back(f); }
    int total = 0;
    for (auto f : foes) { total += f.hp; f.hp = 0; }
    if (total != 6) e++;
    for (auto &f : foes) f.hp += 10;
    total = 0;
    for (const auto &f : foes) total += f.hp;
    if (total != 36) e++;
    std::array<int, 4> nums;
    nums.fill(2);
    int prod = 1;
    for (auto n : nums) prod *= n;
    if (prod != 16) e++;
    std::vector<Enemy *> ptrs;
    ptrs.push_back(&foes[0]);
    for (auto p : ptrs) p->hp = 99;
    for (auto *p : ptrs) p->x = 7;
    if (foes[0].hp != 99 || foes[0].x != 7) e++;
    std::string word = "level";
    int ls = 0;
    for (auto c : word) if (c == 'l') ls++;
    if (ls != 2) e++;
    auto k = twice(4);
    auto fl = 1.5;
    auto *first = &foes[0];
    auto &second = foes[1];
    second.hp = 5;
    auto name = word + "s";
    auto it = foes.begin();
    if (k != 8 || fl * 2 != 3.0 || first->hp != 99 || foes[1].hp != 5 || name != "levels" || it->hp != 99) e++;
    test_errors = e;
    return 0;
}
