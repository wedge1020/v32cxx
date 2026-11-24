// *****************************************************************************
//  tests/177sample.cpp — destructors of global objects, and of exit()
//
//  Global objects (and arrays of them, any number of dimensions) are built at
//  the top of main() and destroyed when main() ends, in the reverse order of
//  construction, after main's own locals. Where the program carries v32libc's
//  v32_exit()/v32_exit_destructors/v32_exit_hook (here a stand-in with the same names), exit()
//  and abort() run them too, before the hook, exactly once: v32_exit never
//  returns in the real library, so main() cannot check what happens after it.
//
//  What this program can check from main() is checked (test_errors); the
//  whole log, including what main's return runs, is compared by
//  tests/globals_destroy.sh, which builds this file as standard C and runs it
//  under a driver (tests/177driver.c). The same script runs 177ret.cpp and
//  177end.cpp: a `return` with a value, and falling off the end of main().
// *****************************************************************************

int test_errors = -1;
int g_log[40];
int g_n = 0;
int g_want[40];
int g_want_n = 0;
int g_want_ret = -1;   // main's return value the driver must see (-1: not checked)

class D {
public:
    int id;
    D() { id = 9; g_log[g_n] = 9; g_n = g_n + 1; }
    D(int i) { id = i; g_log[g_n] = i; g_n = g_n + 1; }
    ~D() { g_log[g_n] = 0 - id; g_n = g_n + 1; }
};

// no destructor, no constructor: nothing to run for these
class Quiet { public: int v; };
Quiet g_quiet;

D g_a(1);
D g_grid[2][2] = { { D(2), { 3 } }, { D(4) } };   // [1][1] is default-constructed (9)
D g_b(5);

// the stand-in for libc's exit machinery: the hook is called by v32_exit()
void (*v32_exit_hook)(int status) = 0;
void (*v32_exit_destructors)(void) = 0;
void v32_exit(int status)
{
    if (v32_exit_destructors != 0) v32_exit_destructors();
    if (v32_exit_hook != 0) v32_exit_hook(status);
}

int hook_n = -1;
void my_hook(int status) { hook_n = g_n; }

int main(void)
{
    int e = 0;
    D loc(7);

    // built before main's body, in declaration order
    if (g_n != 7) e++;
    int want_built[7];
    want_built[0] = 1; want_built[1] = 2; want_built[2] = 3; want_built[3] = 4;
    want_built[4] = 9; want_built[5] = 5; want_built[6] = 7;
    for (int i = 0; i < 7; i++) if (g_log[i] != want_built[i]) e++;

    v32_exit_hook = my_hook;
    v32_exit(0);

    // exit() ran the destructors, last constructed first, before the hook
    int want_exit[6];
    want_exit[0] = -5; want_exit[1] = -9; want_exit[2] = -4; want_exit[3] = -3;
    want_exit[4] = -2; want_exit[5] = -1;
    if (g_n != 13) e++;
    for (int i = 0; i < 6; i++) if (g_log[7 + i] != want_exit[i]) e++;
    if (hook_n != 13) e++;

    // a second exit(), or main's own return, runs them no more
    v32_exit(0);
    if (g_n != 13) e++;

    // what the log must hold once main has returned: ... then the local's
    for (int i = 0; i < 13; i++) g_want[i] = g_log[i];
    g_want[13] = -7;
    g_want_n = 14;
    g_want_ret = 0;

    test_errors = e;
    return 0;
}
