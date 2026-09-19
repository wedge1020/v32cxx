// sample74.cpp -- multiple declarators in one statement (`int a, b, c;`),
// the classic C/C++ idiom. Exercises: plain multi-declarator locals with
// mixed initializers, a pointer among plain declarators (pointer-ness is
// per-declarator, matching real C++: `int *p, q;` makes `q` a plain int,
// not another pointer), globals, and class members.
//
// Expected: total = 1 + 2 + 3 + 40 (via *pv) + 100 (global g2) = 146

int g1 = 100, g2 = 200, g3;

class Pair {
public:
    int x, y;
};

int main() {
    int a = 1, b = 2, c = 3;

    int v = 40, *pv, w;
    pv = &v;

    Pair p;
    p.x = 0;
    p.y = 0;

    int total = a + b + c + *pv + g1;
    return total;
}
