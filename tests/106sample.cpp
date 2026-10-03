// *****************************************************************************
//  tests/106sample.cpp — values bound to reference parameters
//
//  `scale(v, 3)` where the parameter is `const int &factor`: a reference
//  needs an object to refer to, and a literal has none. The generated C
//  used to take the literal's address, `(&3)`. C++ makes an unnamed
//  temporary, and so does v32c++ now:
//      int __v32_ref_tmp0;  ...  scale(v, (__v32_ref_tmp0 = 3, &__v32_ref_tmp0));
//   1. literals (int, float), arithmetic, a function's result, a cast;
//   2. two temporaries in one call, and in nested calls;
//   3. in an `if` and a loop condition, where the value must be
//      recomputed every time round;
//   4. methods and overloaded operators as well as free functions;
//   5. real variables are still passed by address, not copied.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;

int   twice(const int &n)                 { return n * 2; }
int   add(const int &a, const int &b)     { return a + b; }
float half(const float &f)                { return f / 2; }
int   three()                             { return 3; }
void  bump(int &n)                        { n++; }
bool  below(const int &a, const int &b)   { return a < b; }

class Bag
{
public:
    int items[8];
    int count;
    Bag() { count = 0; }
    void push_back(const int &v)   { items[count] = v; count++; }
    Bag &operator+=(const int &v)  { push_back(v); return *this; }
    int sum() { int t = 0; for (int i = 0; i < count; i++) t += items[i]; return t; }
};

int main()
{
    int errors = 0;
    int x = 5;

    if (twice(21) != 42) errors++;
    if (twice(x + 1) != 12) errors++;
    if (twice(three()) != 6) errors++;
    if (twice((int)2.9) != 4) errors++;
    if (half(3) != 1.5) errors++;
    if (add(1, 2) != 3) errors++;
    if (add(twice(2), twice(add(1, 1))) != 8) errors++;

    // a real variable still goes by address
    bump(x);
    if (x != 6) errors++;
    if (twice(x) != 12) errors++;

    // conditions: recomputed on every pass
    int loops = 0;
    for (int i = 0; below(i * 2, 6); i++) loops++;
    if (loops != 3) errors++;
    int n = 0;
    while (below(n + 1, 5)) n++;
    if (n != 4) errors++;
    if (below(x, 100) && below(x + 100, 7)) errors++;

    // methods and operators
    Bag bag;
    bag.push_back(5);
    bag.push_back(x * 2);
    bag += 3;
    if (bag.count != 3 || bag.sum() != 20) errors++;

    test_errors = errors;
    return 0;
}
