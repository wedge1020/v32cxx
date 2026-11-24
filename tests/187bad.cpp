// tests/187bad.cpp -- list-initialized array new: what is refused
class NoDefault {
public:
    int v;
    NoDefault(int x) { v = x; }
};

class Bare {
public:
    int a;
    int b;
};

int main(void)
{
    NoDefault *p = new NoDefault[3]{ 1, 2 };    // the third element needs a default constructor
    Bare *q = new Bare[2]{ {1, 2} };            // no constructor: nothing to build {1, 2} with
    int *r = new int[2]{ {1} };                 // a scalar takes a value
    return 0;
}
