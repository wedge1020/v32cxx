// tests/187bad.cpp -- list-initialized array new: what is refused
class NoDefault {
public:
    int v;
    NoDefault(int x) { v = x; }
};

class Bare {
public:
    int a;
    int b; virtual int sum() { return a + b; }    // not an aggregate: no list can build it
};

int main(void)
{
    NoDefault *p = new NoDefault[3]{ 1, 2 };    // the third element needs a default constructor
    Bare *q = new Bare[2]{ {1, 2} };            // not an aggregate: nothing to build {1, 2} with
    int *r = new int[2]{ {1} };                 // a scalar takes a value
    return 0;
}
