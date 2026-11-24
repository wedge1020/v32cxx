// A `return` with a value from main() and from a nested block: the value is
// computed before the global destructors run, and main's local goes first.
// Run by tests/globals_destroy.sh (see tests/177sample.cpp).
int test_errors = -1;
int g_log[16];
int g_n = 0;
int g_want[16];
int g_want_n = 0;
int g_want_ret = -1;   // main's return value the driver must see (-1: not checked)

class D {
public:
    int id;
    D(int i) { id = i; g_log[g_n] = i; g_n = g_n + 1; }
    ~D() { g_log[g_n] = 0 - id; g_n = g_n + 1; }
};

D g_first(1);
D g_second(2);

int main(void)
{
    D loc(3);
    g_want[0] = 1; g_want[1] = 2; g_want[2] = 3; g_want[3] = -3; g_want[4] = -2; g_want[5] = -1;
    g_want_n = 6;
    g_want_ret = 3;
    test_errors = 0;
    if (g_n == 3) {
        // the value read here must be the one before any destructor ran: 3
        return g_n;
    }
    test_errors = 1;
    return 0;
}
