/* tests/167sample.c -- unsigned `<`, `>`, `<=`, `>=`, `/`, `%` above
 * INT_MAX (P-C7): Vircon32's integer is signed, so these used to read a
 * value like 0x90000000 as negative. Self-checking. */
typedef unsigned int uint;

int test_errors = -1;

uint big (void) { return 0x90000000; }

void main (void)
{
    int e = 0;
    uint a = big ();            /* 2415919104 */
    uint b = 5;
    uint c = 0xF0000001;        /* 4026531841 */
    uint z = 0;

    if (!(a > b)) e |= 1;
    if (a < b) e |= 2;
    if (!(b < a)) e |= 4;
    if (!(a >= 0x80000000)) e |= 8;
    if (!(b <= a)) e |= 16;
    if (!(c > a)) e |= 32;
    if (a < 0) e |= 64;                  /* never below zero */
    if (!(a >= 0)) e |= 128;
    if (!(a > 0)) e |= 256;
    if (z > 0) e |= 512;
    if (!(z <= 0)) e |= 1024;

    if (a / 2 != 0x48000000) e |= 2048;                  /* shift */
    if (a % 16 != 0) e |= 4096;                          /* mask */
    if (c % 16 != 1) e |= 8192;
    if (a / 3 != 805306368) e |= 16384;                  /* 2415919104 / 3 */
    if (a % 7 != 2415919104u % 7) e |= 32768;
    if (c / 0x80000000 != 1) e |= 65536;                 /* divisor above INT_MAX */
    if (a / c != 0) e |= 131072;
    if (c / a != 1) e |= 262144;
    if (c % a != 0x60000001) e |= 524288;                /* 4026531841 - 2415919104 */
    {
        uint d = c;
        d /= 10;
        if (d != 402653184) e |= 1048576;
        d = c;
        d %= 10;
        if (d != 1) e |= 2097152;
    }
    if (-8 / 2 != -4) e |= 4194304;                      /* signed code unchanged */
    if (7 % -4 != 3) e |= 8388608;
    test_errors = e;
}
