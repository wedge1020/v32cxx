// A bare enumerator of an `enum class` (`Red`, not `Color::Red`) is not a name: 205bad.cpp checks the error.
// Names that mean something else still work: a variable, parameter, member, function or plain-enum constant
// that happens to be called like an enumerator.
enum class Color { Red, Green, Blue = Red + 5 };     // inside the enum, the bare name is fine
enum class Shade { Red, Dark };
enum Plain { Pale, Bold };
int Red = 7;                                         // a variable called Red
int Dark(int x) { return x + 1; }                    // a function called like Shade::Dark
struct S { int Green; int get() { return Green; } };
namespace a { enum class K { Two, Red }; int Two() { return 3; } int h() { return Two(); } }
int param(int Blue) { return Blue * 2; }
int test_errors = -1;
int main() {
    int e = 0;
    Color c = Color::Green;
    Shade s = Shade::Red;
    int Pale_copy = Pale;
    S o; o.Green = 4;
    if (Red != 7) e++;
    if (Dark(1) != 2) e++;
    if (o.get() != 4) e++;
    if (a::h() != 3) e++;
    if (param(5) != 10) e++;
    if ((int)c != 1 || (int)s != 0) e++;
    if ((int)Color::Blue != 5) e++;
    if ((int)a::K::Red != 1) e++;
    if (Pale_copy != 0) e++;
    int Blue = 9;                                    // a local called Blue
    if (Blue != 9) e++;
    test_errors = e;
    return 0;
}
