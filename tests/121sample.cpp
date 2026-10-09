// *****************************************************************************
//  tests/121sample.cpp — C++11 virt-specifiers, `explicit`, and `double`
//
//   - `override` and `final` after a member function's parameter list,
//     and `final` after a class's name, are accepted and checked (an
//     `override` that overrides nothing, overriding a `final` function and
//     deriving from a `final` class are errors -- not exercised here,
//     since an error stops the transpile);
//   - they are contextual keywords: `final` and `override` stay ordinary
//     names everywhere else;
//   - `explicit` is accepted and dropped;
//   - `double` and `long double` are float, Vircon32's only floating type.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;
int final = 3;                       // still an ordinary name
int override = 4;
class Shape
{
    public:
        explicit Shape(int s) { side = s; }
        virtual ~Shape() {}
        virtual int area() const { return 0; }
        virtual int sides() { return 0; }
        int side;
};
class Square final : public Shape
{
    public:
        explicit Square(int s) : Shape(s) {}
        int area() const override { return side * side; }
        int sides() override final { return 4; }
};
struct Tag final { int v; };
int main()
{
    int e = 0;
    Square sq(3);
    Shape *s = &sq;
    if (s->area() != 9 || s->sides() != 4) e++;
    if (final + override != 7) e++;
    if (override) final = 0;
    Tag t; t.v = 1;
    if (final != 0 || t.v != 1) e++;
    double d = 1.5; long double ld = 2.0;
    double half = d / 3.0;
    if (half != 0.5 || d * ld != 3.0) e++;
    test_errors = e;
    return 0;
}
