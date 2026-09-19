// Companion NEGATIVE test to tests/79sample.cpp's own `friend` support:
// confirms that a class NOT named in a `friend` declaration still gets
// this project's ordinary, pre-existing private-access error -- the
// same check_member_access this project already had, now with
// is_friend_of() consulted first (sema.c) and correctly returning
// false here, since Box friends only BoxPrinter (tests/79sample.cpp),
// never Snoop.
//
// Expected (and REQUIRED for `make test` to pass): a parse-time-clean,
// semantic-analysis-time ERROR -- "'value' is a private member of
// class 'Box' and cannot be accessed here" -- exactly matching the
// wording tests/10sample.cpp's own pre-existing private-access
// negative test already uses, confirming `friend` support didn't
// accidentally loosen access control for everyone, only for classes
// and functions actually named `friend`.

class Box {
    friend class BoxPrinter;

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

class Snoop {
    public:
        int peek(const Box &b);
};

int Snoop::peek(const Box &b) {
    return b.value;
}

void main() {
    Box a(5);
    Snoop s;
    int x = s.peek(a);
}
