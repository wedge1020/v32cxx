// Exercises the reference-parameter call-site lowering fix: a C++
// reference parameter (`Shape &s`) implicitly takes the address of
// whatever's passed at the call site -- this project's own lowering
// previously never inserted that address-of once the parameter itself
// was relabeled to a plain C pointer (fix_references, lower.c, only
// ever rewrote `.` to `->` access INSIDE the callee's own body, with
// no visibility into any CALL SITE at all). Fixed in finalize_call
// (lower.c): for each argument whose corresponding parameter was
// declared as a reference, wrap it in address_of_if_needed the same
// way a method's own receiver already was.
//
// Also exercises a real, separate, narrower gap found while fixing
// this: infer_expr_type (sema.c) never handled AST_UNOP at all (`*p`,
// `&x`), so address_of_if_needed's own "already a pointer, don't
// double-address" check silently treated a dereferenced-pointer
// argument as "unknown type, don't wrap" -- passing an ungenerated,
// wrong `Shape` value where `Shape *` was needed. Fixed alongside this
// round; `getSizeViaDeref` below exercises it directly.
//
// Expected: total = 12 (7 + 5, via a plain-variable reference
// argument and a dereferenced-pointer reference argument).

class Shape {
    public:
        Shape();
        int size;
};

Shape::Shape() {
    this->size = 0;
}

int getSize(Shape &s) {
    return s.size;
}

int getSizeViaDeref(Shape &s) {
    return s.size;
}

void main() {
    Shape a;
    a.size = 7;
    int first = getSize(a);

    Shape *b = new Shape();
    b->size = 5;
    int second = getSizeViaDeref(*b);

    int total = first + second;
}
