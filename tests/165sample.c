/* tests/165sample.c -- --warn-error: this file draws the `unsigned` warning
 * (kind "unsigned"); built plainly it works, built with --warn-error or
 * --warn-error=unsigned it must fail and leave no output. Self-checking. */
int test_errors = -1;

void main (void)
{
    unsigned n = 5;
    test_errors = (n + 1 == 6) ? 0 : 1;
}
