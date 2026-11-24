enum class Color { Red, Green };
enum class Shade { Red, Dark };
namespace a { enum class K { Two }; int g() { return Two; } }
int main() {
    Color c = Red;
    Color d = Green;
    int x = Dark + 1;
    return 0;
}
