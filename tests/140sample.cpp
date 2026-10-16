/* *****************************************************************************
 *  tests/140sample.cpp -- out-of-line definitions may rename the parameters
 *
 *  C++ lets `int f(int dir)` in the class and `int A::f(int direction)` in the
 *  definition spell a parameter differently; the body uses the definition's
 *  names. Same for constructors, with a member-initializer list.
 *  Self-checking: test_errors must end at 0.
 * ***************************************************************************** */

int test_errors = -1;

class A
{
public:
    int v;
    int w;

    A ( int first, int second );
    int  f ( int dir, int type );
    void g ( int a );
};

A::A ( int start, int finish ) : v(start), w(finish) { }
int  A::f ( int direction, int kind ) { return direction * 10 + kind + v; }
void A::g ( int b ) { v = b; }

int main ()
{
    int e = 0;
    A a( 1, 2 );

    if ( a.v != 1 || a.w != 2 ) e += 1;
    if ( a.f( 3, 4 ) != 35 ) e += 2;
    a.g( 7 );
    if ( a.v != 7 ) e += 4;

    test_errors = e;
    return 0;
}
