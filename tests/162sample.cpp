/* tests/162sample.cpp -- C++ linking: a class declared in a header that both
 * files include (read once), its methods defined in the other file, a
 * file-private static of the same name in each. Self-checking. */
#include "162shape.hpp"
int test_errors = -1;
static int calls = 5;
int main() {
    Counter c(10);
    int e = 0;
    if (c.next() != 10) e |= 1;
    if (c.next() != 11 || c.peek() != 12) e |= 2;
    if (twice(21) != 42) e |= 4;
    if (g_total != 101 + 102) e |= 8;
    if (calls != 5) e |= 16;
    test_errors = e;
    return 0;
}
