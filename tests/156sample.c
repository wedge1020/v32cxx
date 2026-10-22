/* tests/156sample.c -- designated initializers (gap report G2)
 * `[index] = v`, `.member = v`, nested designators and out-of-order items,
 * for dispatch tables (function-pointer arrays with [] sized by the
 * highest index), structs, arrays of structs and locals. Self-checking:
 * test_errors must end at 0. */
int test_errors = -1;

enum { SYS_a = 1, SYS_b, SYS_c = 4, NSYS };
#define IDX_X 2

struct S { int a; int b; };
struct T { struct S s; int c[3]; int *p; };

static int f0 (void) { return 10; }
static int f1 (void) { return 11; }
static int f2 (void) { return 12; }

static int (*ord[])(void) = { [1] = f1, [0] = f0 };
static int (*sys[])(void) = { [SYS_a] = f1, [SYS_c] = f2 };
static struct S s = { .b = 2, .a = 1 };
static struct T tt = { .c = { 5, 6, 7 }, .s.b = 9 };
static struct T tu = { .c[1] = 4, .s = { 1, 2 } };
static struct S arr[4] = { [2] = { .b = 3 }, [0].a = 7 };
static int plain[6] = { 1, 2, [IDX_X + 2] = 9, 10 };
static float fl[3] = { [2] = 1.5f };
static const char *names[NSYS] = { [SYS_b] = "b", [SYS_c] = "c" };

void main (void)
{
    int loc[5] = { [3] = 8 };
    struct S ls = { .a = 5 };
    struct S two[2] = { [1] = { .a = 1, .b = 2 } };
    int e = 0;

    if (sizeof (ord) != 2 * sizeof (ord[0])) e |= 1;
    if (ord[0] () != 10 || ord[1] () != 11) e |= 2;
    if (sizeof (sys) != 5 * sizeof (sys[0])) e |= 4;
    if (sys[1] () != 11 || sys[4] () != 12) e |= 8;
    if (sys[0] || sys[2] || sys[3]) e |= 16;
    if (s.a != 1 || s.b != 2) e |= 32;
    if (tt.c[2] != 7 || tt.s.b != 9 || tt.s.a != 0 || tt.p) e |= 64;
    if (tu.c[1] != 4 || tu.c[0] != 0 || tu.s.b != 2 || tu.s.a != 1) e |= 128;
    if (arr[2].b != 3 || arr[2].a != 0 || arr[0].a != 7 || arr[1].a != 0 || arr[3].b != 0) e |= 256;
    if (plain[0] != 1 || plain[1] != 2 || plain[2] != 0 || plain[3] != 0 || plain[4] != 9 || plain[5] != 10) e |= 512;
    if (loc[3] != 8 || loc[0] != 0 || ls.a != 5 || ls.b != 0) e |= 1024;
    if (two[0].a != 0 || two[1].a != 1 || two[1].b != 2) e |= 2048;
    if (fl[0] != 0.0f || fl[2] != 1.5f) e |= 4096;
    if (names[0] || names[1] || names[SYS_b][0] != 'b' || names[SYS_c][0] != 'c') e |= 8192;
    test_errors = e;
}
