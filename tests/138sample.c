/* *****************************************************************************
 *  tests/138sample.c -- signed right shift, with C input
 *
 *  `>>` of a signed value is arithmetic, as in C; `unsigned` stays logical.
 *  See tests/136sample.cpp, and docs/VIRCON32_QUIRKS.md #14.
 *  Self-checking: test_errors must end at 0.
 * ***************************************************************************** */

int test_errors = -1;

int table[4];
unsigned bits = 0xF0000000u;

int neg(void) { return -24; }

void main(void)
{
    int e = 0;
    int x = -8;
    int *p = &x;
    unsigned u = 0x80000000u;

    table[1] = -17;
    if ((x >> 1) != -4) e += 1;
    if ((table[1] >> 2) != -5) e += 2;
    if ((*p >> 3) != -1) e += 4;
    if ((neg() >> 2) != -6) e += 8;
    if ((u >> 31) != 1) e += 16;
    if ((bits >> 28) != 15) e += 32;
    x >>= 2;
    if (x != -2) e += 64;
    table[1] >>= 1;
    if (table[1] != -9) e += 128;
    if ((-8 >> 1) != -4) e += 256;
    if ((-2147483647 - 1) >> 31 != -1) e += 512;

    test_errors = e;
}
