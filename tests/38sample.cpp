// Deliberately invalid: a member-initializer list naming an actual data
// member whose own type IS a class (not a pointer/reference to one) --
// valid C++, but real, separate complexity (invoking the member's own
// constructor) this project doesn't support anywhere yet, unlike a
// primitive member field (see sample37.cpp) or base-class delegation
// (see sample35.cpp). Should produce exactly one semantic error, from
// resolve_member_init_list's own "class-typed fields aren't supported
// yet" branch -- not a parse failure, not silent acceptance, and not
// the "not a base class or member" branch either (`thing` IS a real,
// declared member -- just one this round's own scope doesn't reach).

class Inner {
    public:
        Inner(int v);
    private:
        int value;
};

Inner::Inner(int v) {
    this->value = v;
}

class Outer {
    public:
        Outer(int v);
    private:
        Inner thing;
};

Outer::Outer(int v) : thing(v) {
}

void main() {
}
