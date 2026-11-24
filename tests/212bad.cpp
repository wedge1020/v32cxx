// leading `::`: `::new`, `::delete` and `::operator` are refused with a message
namespace a { int f(int v) { return v; } }
int main() {
    int *p = ::new int(5);
    return 0;
}
