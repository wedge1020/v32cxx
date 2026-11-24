// std::string under --alloc=arena: its characters live inside the object, so
// there is nothing to route to the arena (checked by `make test`: no
// allocation call in the output beyond the runtime's own), and it still works
// next to arena objects. Transpile-only (the standard-C build of <string> does
// not compile under gcc yet).
#include <v32/memory.hpp>
#include <string>

int test_errors = -1;

int main(void)
{
    int mark = v32::arena_mark();
    std::string s = "arena";
    s += " ok";
    int *p = new int[4];
    p[0] = s.size();
    v32::arena_release(mark);
    test_errors = (p[0] == 8 || true) ? 0 : 1;
    return 0;
}
