/* Driver for tests/globals_destroy.sh: main() of the program under test is
 * renamed real_main; once it has returned (so every destructor of a global
 * has run) the log it kept must be the one it said to expect. */
#include <stdio.h>
extern int test_errors;
extern int g_log[];
extern int g_n;
extern int g_want[];
extern int g_want_n;
extern int g_want_ret;
int real_main(void);

int main(void)
{
    int ret = real_main();
    int bad = (test_errors != 0) || (g_n != g_want_n) || (g_want_ret >= 0 && ret != g_want_ret);
    for (int i = 0; i < g_n && i < g_want_n; i++)
        if (g_log[i] != g_want[i]) bad = 1;
    if (bad) {
        printf("FAIL: returned %d, test_errors=%d, log of %d entries (want %d):", ret, test_errors, g_n, g_want_n);
        for (int i = 0; i < g_n; i++) printf(" %d", g_log[i]);
        printf("\n");
        return 1;
    }
    return 0;
}
