// *****************************************************************************
//  tests/186sample.cpp -- objects declared inside a namespace
//
//  Global objects in a namespace (nested, or in a namespace reopened later) are
//  built at the top of main() and destroyed when main() ends exactly like file-scope
//  ones: declaration order, namespaces at their own place in the source, and the
//  destructors in the reverse order after main's locals. Arrays take their brace
//  lists. Run by tests/globals_destroy.sh with tests/177driver.c: only a driver
//  outside main() sees the destructors that main's end runs.
// *****************************************************************************

int test_errors = -1;
int g_log[40];
int g_n = 0;
int g_want[40];
int g_want_n = 0;
int g_want_ret = -1;

class Counter {
public:
    int id;
    int hits;
    Counter() { id = 9; hits = 0; g_log[g_n] = 9; g_n = g_n + 1; }
    Counter(int i) { id = i; hits = 0; g_log[g_n] = i; g_n = g_n + 1; }
    ~Counter() { g_log[g_n] = 0 - id; g_n = g_n + 1; }
    void hit() { hits = hits + 1; }
};

Counter t1(1);

namespace game {
    Counter g(2);
    Counter arr[2] = { Counter(3), { 4 } };
    Counter *pg = 0;                      // a pointer: nothing to run
    namespace inner {
        Counter deep(5);
    }
}

Counter t2(6);

namespace game {                          // reopened: its objects come at this place
    Counter again(7);
    Counter blank;                        // the default constructor
}

namespace other {
    int plain = 3;
    void touch() { game::g.hit(); game::inner::deep.hit(); }
}

int main(void)
{
    int e = 0;
    Counter loc(8);

    // all built before main's body, in source order
    int want_built[9] = { 1, 2, 3, 4, 5, 6, 7, 9, 8 };
    if (g_n != 9) e++;
    for (int i = 0; i < 9 && i < g_n; i++)
        if (g_log[i] != want_built[i]) e++;

    // usable by qualified name, methods included
    if (game::g.id != 2 || game::arr[1].id != 4 || game::inner::deep.id != 5) e++;
    if (game::again.id != 7 || game::blank.id != 9 || other::plain != 3) e++;
    other::touch();
    game::g.hit();
    if (game::g.hits != 2 || game::inner::deep.hits != 1) e++;
    if (game::pg != 0) e++;

    // what the log must hold once main has returned: then the local's, then the globals' in reverse
    for (int i = 0; i < 9; i++) g_want[i] = g_log[i];
    int want_end[9] = { -8, -9, -7, -6, -5, -4, -3, -2, -1 };
    for (int i = 0; i < 9; i++) g_want[9 + i] = want_end[i];
    g_want_n = 18;
    g_want_ret = 0;

    test_errors = e;
    return 0;
}
