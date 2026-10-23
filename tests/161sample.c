/* tests/161sample.c -- gaps found by running xv6's user programs through
 * the real Vircon32 compiler:
 *  - `(p = f()) == 0`, `(char *)q == 0`: the zero is NULL even when the
 *    other side is an assignment or a cast
 *  - a literal 0 argument for a pointer parameter (`pipe (0)`)
 *  - `const char *` read into a `char` (the SDK compiler enforces const)
 *  - a `struct cpu *` member where no `struct cpu` is ever defined
 *  - `struct pipe;` beside `int pipe (int *)`
 *  - `ptr < 0` and `(void *)p + n`
 *  - an unguarded header included by both files
 *  - GCC's `__atomic_*` builtins (one CPU: the plain operations)
 *  - `(char *) p < array`.
 * Self-checking: test_errors must end at 0. */
#include "161types.h"

int test_errors = -1;

struct spinlock { uint locked; char *name; struct cpu *cpu; };
struct spinlock lk;

struct pipe;
int pipe (int *fds);
char *scan_to_end (char *s, int n);

static char buf[16];
static int lockword;
static char *last;

static char *lookup (int i)
{
    if (i == 0) return 0;
    return &buf[i];
}

static void copy (char *dst, const char *src)
{
    while ((*dst++ = *src++) != 0)
        ;
}

void main (void)
{
    int e = 0;
    int fds[2];
    char *p;
    str_t q;

    copy (buf, "ab");
    if (buf[0] != 'a' || buf[1] != 'b' || buf[2] != 0) e |= 1;

    if ((p = lookup (0)) == 0) { } else e |= 2;
    if ((last = lookup (3)) == 0) e |= 4;
    if (((char *) p) != 0) e |= 8;

    if (pipe (0) != -1) e |= 16;
    if (pipe (fds) != 0 || fds[0] != 3 || fds[1] != 4) e |= 32;

    lk.cpu = 0;
    if (lk.cpu != 0) e |= 64;

    q = buf;
    if (scan_to_end (q, 1) != &buf[1]) e |= 128;
    if (((void *) q) + 1 != &buf[1]) e |= 256;

    p = 0;
    if (p < 0) e |= 512;

    lockword = 5;
    if (__atomic_exchange_n (&lockword, 1, __ATOMIC_ACQUIRE) != 5 || lockword != 1) e |= 1024;
    __atomic_store_n (&lockword, 0, __ATOMIC_RELEASE);
    if (__atomic_load_n (&lockword, __ATOMIC_ACQUIRE) != 0) e |= 2048;
    __sync_synchronize ();

    if (!((char *) &buf[2] < buf + 4)) e |= 4096;
    if (!((char *) &buf[2] < &buf[4])) e |= 8192;

    test_errors = e;
}
