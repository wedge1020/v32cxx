/* Driver for tests/std_run.sh: runs the program under test (its main() renamed
 * real_main) built as standard C, and fails if it set test_errors to anything
 * but 0 (or never set it). */
#include <stdio.h>
extern int test_errors;
int real_main(void);

int main(void)
{
    int ret = real_main();
    (void)ret;
    if (test_errors != 0) {
        printf("FAIL: test_errors = %d\n", test_errors);
        return 1;
    }
    return 0;
}
