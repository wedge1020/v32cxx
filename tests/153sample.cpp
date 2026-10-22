/* *****************************************************************************
 *  tests/153sample.cpp -- --silence-unused-args
 *
 *  The Vircon32 compiler warns "argument X is not used" for a method that
 *  touches no member (its hidden `this`) and for a callback parameter the
 *  function-pointer type requires but the body ignores. With
 *  --silence-unused-args each such function starts with a bare `X;`, which
 *  the compiler counts as a use. The Makefile checks the generated C, with
 *  and without the option. Self-checking: test_errors must end at 0.
 * ***************************************************************************** */

int test_errors = -1;
int hits = 0;

void bump (void) { hits++; }

class Counter
{
public:
    int total;
    void tick (void) { bump (); }              /* uses no member: `this` unused */
    int value (void) { return total; }         /* uses this */
    int add (int used, int ignored) { return used + total; }
};

void handler (char *tail) { hits += 10; }      /* the type requires the parameter */

void main (void)
{
    Counter c;
    void (*cb) (char *) = handler;
    c.total = 5;
    c.tick ();
    cb (0);
    int e = 0;
    if (hits != 11) e += 1;
    if (c.add (2, 99) != 7) e += 2;
    if (c.value () != 5) e += 4;
    test_errors = e;
}
