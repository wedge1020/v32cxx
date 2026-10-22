/* tests/158sample.c -- gaps G1, G3, G4, G5 of the v32nix report
 *   G1  __attribute__((...)) / __declspec ignored wherever it stands
 *   G3  compound literals (T){...} and (T[]){...}
 *   G4  enums defined inside a function body (two functions reuse `N`)
 *   G5  _Static_assert / static_assert, checked while transpiling
 *   G8  a parameter of function type, `int call (int f (int), int v)`
 * Self-checking: test_errors must end at 0. */
int test_errors = -1;

int exit (int) __attribute__((noreturn));
void panic (char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
struct D { char name[14] __attribute__((nonstring)); int n; };
__attribute__((aligned(16))) int stack0[16];
static __attribute__((unused)) int one (void) { return 1; }

struct P { int x; int y; };
enum { BSIZE = 64 };
_Static_assert (sizeof (struct P) * 4 <= BSIZE, "P table too big");
static_assert (BSIZE == 64);

static int sum (int *v, int n) { int s = 0; for (int i = 0; i < n; i++) s += v[i]; return s; }
static int count (char **argv) { int n = 0; while (argv[n]) n++; return n; }
static int px (struct P p) { return p.x * 10 + p.y; }

static int twice (int v) { return v * 2; }
static int call (int fn (int), int v) { return fn (v) + 1; }

static int f (void)
{
    enum { N = 250, SZ = 4 };
    char buf[SZ];
    _Static_assert (N > 1, "n");
    buf[0] = 5;
    return buf[0] + N + sizeof (buf);
}

static int g (void)
{
    enum { N = 7, M = N + 1 };
    int t[M];
    t[0] = N;
    return t[0] + M + sizeof (t);
}

void main (void)
{
    int a = 3, b = 4, e = 0;
    struct P *pp;

    if (one () != 1 || sizeof (struct D) != 15) e |= 1;
    if (f () != 5 + 250 + 4) e |= 2;
    if (g () != 7 + 8 + 8) e |= 4;
    if (count ((char *[]){"/init", "x", 0}) != 2) e |= 8;
    if (sum ((int[]){a, b, 5}, 3) != 12) e |= 16;
    if (px ((struct P){ .y = b, .x = a }) != 34) e |= 32;
    pp = &(struct P){7, 8};
    if (pp->x != 7 || pp->y != 8) e |= 64;
    if ((int[]){10, 20, 30}[2] != 30) e |= 128;
    for (int i = 0; i < 3; i++)
        if (sum ((int[]){i, i}, 2) != 2 * i) e |= 256;
    if (call (twice, 5) != 11) e |= 512;
    test_errors = e;
}
