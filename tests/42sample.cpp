// Deliberately invalid: the same field named twice in one
// member-initializer list -- ill-formed in real C++ ("multiple
// initializations given for 'x'"), and previously silently accepted
// here too (whichever entry a linear search found first would quietly
// win, no diagnostic at all). Should produce exactly one semantic
// error, for the SECOND occurrence of 'x' specifically.

class Widget {
    public:
        Widget(int a, int b);
    private:
        int x;
};

Widget::Widget(int a, int b) : x(a), x(b) {
}

void main() {
}
