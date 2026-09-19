// Deliberately invalid: a member-initializer list naming something that
// is neither this class's own base class (Widget has none at all) nor a
// declared member -- should produce exactly one semantic error, from
// resolve_member_init_list's own "not a base class or member" branch.

class Widget {
    public:
        Widget(int v);
    private:
        int value;
};

Widget::Widget(int v) : NotARealThing(v) {
    this->value = v;
}

void main() {
}
