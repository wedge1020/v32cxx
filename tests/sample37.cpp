// Deliberately invalid: a member-initializer list naming an ordinary,
// actually-declared data member (not a base class) -- valid C++, but
// not yet supported by this project (only base-class delegation is,
// as of this round). Should produce exactly one semantic error, from
// resolve_member_init_list's own "not yet supported" branch, not a
// parse failure and not silent acceptance.

class Widget {
    public:
        Widget(int v);
    private:
        int value;
};

Widget::Widget(int v) : value(v) {
}

void main() {
}
