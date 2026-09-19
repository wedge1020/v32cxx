// Exercises `friend` -- both a friend CLASS (`friend class BoxPrinter;`)
// and a friend FUNCTION (`friend int compare(const Box &a, const Box
// &b);`) declared inside the same class body -- the last of the flat
// parse rejections this project's own intro-OOP audit turned up
// (README's own "Gaps found auditing common intro-OOP patterns"
// section), and the one the user asked for by name.
//
// New grammar: `member` gained two alternatives, `FRIEND CLASS
// IDENTIFIER ';'` and `FRIEND func_header ';'` -- see parser.y's own
// comments on both for why the class name is a bare IDENTIFIER (not
// TYPE_NAME: friending a class not yet defined earlier in the file is
// the whole point of the feature) and why a friend function gets its
// own AST_FRIEND_FUNC_DECL kind rather than an AST_FUNC_DECL flag bit
// (so it's simply invisible to every existing pass that walks a
// class's own MEMBERS -- it isn't one).
//
// Access-control fix: sema.c's check_member_access now checks a new
// is_friend_of() before enforcing private/protected at all -- a friend
// CLASS is checked by class IDENTITY (ClassLayout.friend_classes), a
// friend FUNCTION by NAME only (ClassLayout.friend_function_names,
// via a new g_current_function_being_checked global tracking which
// function's body is presently being checked) -- see both fields' own
// doc comments in sema.h for the deliberate scope limits (not
// transitive, not inherited, name- rather than signature-matched for
// the function case).
//
// Expected: BoxPrinter::reveal and the free function compare() can
// both read Box's own private `value` field despite being neither
// Box's own method nor Box itself -- r1 = 5, r2 = -4 (5 - 9) -- while
// tests/80sample.cpp (this round's own companion negative test)
// confirms a NON-friend class still gets the ordinary private-access
// error this project already had.

class Box {
    friend class BoxPrinter;
    friend int compare(const Box &a, const Box &b);

    public:
        Box(int v);

    private:
        int value;
};

Box::Box(int v) {
    this->value = v;
}

class BoxPrinter {
    public:
        int reveal(const Box &b);
};

int BoxPrinter::reveal(const Box &b) {
    return b.value;
}

int compare(const Box &a, const Box &b) {
    return a.value - b.value;
}

void main() {
    Box a(5);
    Box b(9);
    BoxPrinter p;

    int r1 = p.reveal(a);
    int r2 = compare(a, b);
}
